#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <fluidsynth.h>

#define NUM_DEVICES 8
#define SENSORS_PER_DEVICE 6
#define MAX_COM_PORTS 32
#define MAX_PATH_LEN 1024

// ---------- FluidSynth globals ----------
static fluid_settings_t* g_settings = NULL;
static fluid_synth_t*    g_synth    = NULL;
static fluid_audio_driver_t* g_adriver = NULL;

// Protect FluidSynth calls (simple, safe)
static CRITICAL_SECTION g_synthLock;

// Track which notes are currently active (to avoid repeats)
static volatile LONG g_running = 1;

// Per-channel (device) soundfont/program config
typedef struct {
    char sf2_path[MAX_PATH_LEN]; // soundfont file
    int  bank;                   // usually 0
    int  program;                // 0-127, default 0
    int  sfont_id;               // set at runtime
} ChannelConfig;

static ChannelConfig g_chan[NUM_DEVICES];

// ----------------- COM port handling -----------------

typedef struct {
    char port_name[64]; // e.g., "COM3" (we'll convert to \\.\COM3)
} SerialThreadArgs;

/* ----- DEVICE CONFIGURATIONS ----- */

typedef struct {
    int pitch;
    int velocity;
    bool holdable;
} Note;

typedef struct {
    Note* notes[SENSORS_PER_DEVICE];
    int velocity_offset;
    uint8_t old_mask;
} Device;

enum {
    OFFSET_0 = 0,
    OFFSET_1 = 1,
    OFFSET_2 = 2,
    OFFSET_3 = 3,
    OFFSET_4 = 4,
    OFFSET_5 = 5,
    OFFSET_6 = 6,
    OFFSET_7 = 7
};

// Victoria
Note note_0_0 = {65, 64, true};
Note note_0_1 = {67, 64, true};
Note note_0_2 = {69, 64, true};
Note note_0_3 = {72, 64, true};
Note note_0_4 = {74, 64, true};
Note note_0_5 = {76, 64, true};

// Edwin
Note note_1_0 = {69, 64, true};
Note note_1_1 = {72, 64, true};
Note note_1_2 = {74, 64, true};
Note note_1_3 = {76, 64, true};
Note note_1_4 = {77, 64, true};
Note note_1_5 = {81, 64, true};

// Yuktha
Note note_2_0 = {65, 64, true};
Note note_2_1 = {67, 64, true};
Note note_2_2 = {69, 64, true};
Note note_2_3 = {72, 64, true};
Note note_2_4 = {74, 64, true};
Note note_2_5 = {76, 64, true};

// Jessica
Note note_3_0 = {67, 64, true};
Note note_3_1 = {67, 64, true};
Note note_3_2 = {67, 64, true};
Note note_3_3 = {67, 64, true};
Note note_3_4 = {67, 64, true};
Note note_3_5 = {67, 64, true};

// Trang
Note note_4_0 = {41, 64, true};
Note note_4_1 = {43, 64, true};
Note note_4_2 = {45, 64, true};
Note note_4_3 = {48, 64, true};
Note note_4_4 = {50, 64, true};
Note note_4_5 = {47, 64, true};

// Maddie
Note note_5_0 = {41, 64, true};
Note note_5_1 = {43, 64, true};
Note note_5_2 = {45, 64, true};
Note note_5_3 = {48, 64, true};
Note note_5_4 = {50, 64, true};
Note note_5_5 = {52, 64, true};

// Mathew
Note note_6_0 = {36, 64, false};
Note note_6_1 = {43, 64, false};
Note note_6_2 = {47, 64, false};
Note note_6_3 = {50, 64, false};
Note note_6_4 = {46, 64, false};
Note note_6_5 = {49, 64, false};

// Audrey
Note note_7_0 = {38, 64, false};
Note note_7_1 = {36, 64, false};
Note note_7_2 = {54, 64, false};
Note note_7_3 = {56, 64, false};
Note note_7_4 = {46, 64, false};
Note note_7_5 = {42, 64, false};

// Devices
Device victoria = {{&note_0_0, &note_0_1, &note_0_2, &note_0_3, &note_0_4, &note_0_5}, OFFSET_0};
Device edwin    = {{&note_1_0, &note_1_1, &note_1_2, &note_1_3, &note_1_4, &note_1_5}, OFFSET_1};
Device yuktha   = {{&note_2_0, &note_2_1, &note_2_2, &note_2_3, &note_2_4, &note_2_5}, OFFSET_2};
Device jessica  = {{&note_3_0, &note_3_1, &note_3_2, &note_3_3, &note_3_4, &note_3_5}, OFFSET_3};
Device trang    = {{&note_4_0, &note_4_1, &note_4_2, &note_4_3, &note_4_4, &note_4_5}, OFFSET_4};
Device maddie   = {{&note_5_0, &note_5_1, &note_5_2, &note_5_3, &note_5_4, &note_5_5}, OFFSET_5};
Device mathew   = {{&note_6_0, &note_6_1, &note_6_2, &note_6_3, &note_6_4, &note_6_5}, OFFSET_6};
Device audrey   = {{&note_7_0, &note_7_1, &note_7_2, &note_7_3, &note_7_4, &note_7_5}, OFFSET_7};

Device* devices[NUM_DEVICES] = {&victoria, &edwin, &yuktha, &jessica, &trang, &maddie, &mathew, &audrey};

// static bool g_active[NUM_DEVICES][SENSORS_PER_DEVICE] = {0};

// // Default note map: 8 devices × 6 sensors → MIDI notes
// // Tune as you like; kept within 0..127.
// static int g_note_map[NUM_DEVICES][SENSORS_PER_DEVICE] = {
// /* dev 0 */ {65, 67, 69, 72, 74, 76}, // victoria: F, G, A, C, D, E (unused)
// /* dev 1 */ {69, 72, 74, 76, 77, 81}, // edwin: A, C, D, E, F, A
// /* dev 2 */ {65, 67, 69, 72, 74, 76}, // yuktha F, G, A, C, D, E (unused)
// /* dev 3 */ {67, 67, 67, 67, 67, 67}, // jessica 67
// /* dev 4 */ {41, 43, 45, 48, 50, 52}, // trang F, G, A, C, D, E (unused)
// /* dev 5 */ {41, 43, 45, 48, 50, 52}, // maddie F, G, A, C, D, E (unused)
// /* dev 6 */ {36, 43, 47, 50, 46, 49}, // mathew Bass, Low Floor, Low-Mid, High, Splash, Crash
// /* dev 7 */ {36, 38, 54, 56, 42, 51}  // audorii Bass, Snare, Tambo, Cowbell, Hi-hat, Ride
// };