/*
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -DCMAKE_BUILD_TYPE=Release

cmake --build build --config Release

.\build\Release\serial_synth.exe COM5 --sf2=boop_bap.sf2

.\build\Release\serial_synth.exe COM5 --sf2=usb.sf2
*/



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

// enum {
//     OFFSET_0 = 32,
//     OFFSET_1 = 32,
//     OFFSET_2 = 16,
//     OFFSET_3 = 0,
//     OFFSET_4 = 0,
//     OFFSET_5 = 63,
//     OFFSET_6 = 0,
//     OFFSET_7 = 0
// };

// // Trang
// Note note_0_0 = {65, 64, false};
// Note note_0_1 = {67, 64, false};
// Note note_0_2 = {69, 64, false};
// Note note_0_3 = {72, 64, false};
// Note note_0_4 = {74, 64, false};
// Note note_0_5 = {76, 64, false};

// // Edwin
// Note note_1_0 = {69, 64, false};
// Note note_1_1 = {72, 64, false};
// Note note_1_2 = {74, 64, false};
// Note note_1_3 = {76, 64, false};
// Note note_1_4 = {77, 64, false};
// Note note_1_5 = {81, 64, false};

// // Victoria
// Note note_2_0 = {65, 64, false};
// Note note_2_1 = {67, 64, false};
// Note note_2_2 = {69, 64, false};
// Note note_2_3 = {72, 64, false};
// Note note_2_4 = {74, 64, false};
// Note note_2_5 = {76, 64, false};

// // Yuktha
// Note note_3_0 = {65, 64, false};
// Note note_3_1 = {69, 64, false};
// Note note_3_2 = {72, 64, false};
// Note note_3_3 = {74, 64, false};
// Note note_3_4 = {76, 64, false};
// Note note_3_5 = {77, 64, false};

// // Jessica
// Note note_4_0 = {41, 64, false};
// Note note_4_1 = {43, 64, false};
// Note note_4_2 = {45, 64, false};
// Note note_4_3 = {48, 64, false};
// Note note_4_4 = {50, 64, false};
// Note note_4_5 = {47, 64, false};

// // Maddie
// Note note_5_0 = {41, 64, false};
// Note note_5_1 = {43, 64, false};
// Note note_5_2 = {45, 64, false};
// Note note_5_3 = {50, 64, false};
// Note note_5_4 = {52, 64, false};
// Note note_5_5 = {48, 64, false};

// // Mathew
// Note note_6_0 = {43, 64, false};
// Note note_6_1 = {47, 64, false};
// Note note_6_2 = {50, 64, false};
// Note note_6_3 = {46, 64, false};
// Note note_6_4 = {36, 64, false};
// Note note_6_5 = {49, 64, false};

// // Audrey
// Note note_7_0 = {38, 64, false};
// Note note_7_1 = {36, 64, false};
// Note note_7_2 = {54, 64, false};
// Note note_7_3 = {56, 64, false};
// Note note_7_4 = {46, 64, false};
// Note note_7_5 = {42, 64, false};



// enum {
//     OFFSET_0 = 32,
//     OFFSET_1 = 0,
//     OFFSET_2 = 16,
//     OFFSET_3 = 0,
//     OFFSET_4 = -16,
//     OFFSET_5 = -16,
//     OFFSET_6 = 32,
//     OFFSET_7 = -16
// };

// // Trang (0)
// Note note_0_0 = {45, 64, true};
// Note note_0_1 = {48, 64, true};
// Note note_0_2 = {52, 64, true};
// Note note_0_3 = {53, 64, true};
// Note note_0_4 = {56, 64, true};
// Note note_0_5 = {57, 64, true};

// // Edwin (1)
// Note note_1_0 = {64, 64, true};
// Note note_1_1 = {65, 64, true};
// Note note_1_2 = {69, 64, true};
// Note note_1_3 = {72, 64, true};
// Note note_1_4 = {76, 64, true};
// Note note_1_5 = {81, 64, true};

// // Victoria (2)
// Note note_2_0 = {36, 64, false};
// Note note_2_1 = {38, 64, false};
// Note note_2_2 = {42, 64, false};
// Note note_2_3 = {46, 64, false};
// Note note_2_4 = {41, 64, false}; // Unused
// Note note_2_5 = {43, 64, false}; // Unused

// // Yuktha (3)
// Note note_3_0 = {64, 64, false};
// Note note_3_1 = {65, 64, false};
// Note note_3_2 = {69, 64, false};
// Note note_3_3 = {72, 64, false};
// Note note_3_4 = {74, 64, false};
// Note note_3_5 = {76, 64, false};

// // Jessica (4)
// Note note_4_0 = {45, 80, false};
// Note note_4_1 = {48, 64, false};
// Note note_4_2 = {52, 64, false};
// Note note_4_3 = {45, 64, false};
// Note note_4_4 = {48, 64, false};
// Note note_4_5 = {52, 64, false};

// // Maddie (5)
// Note note_5_0 = {36, 80, false};
// Note note_5_1 = {38, 64, false};
// Note note_5_2 = {36, 64, false}; // Unused
// Note note_5_3 = {36, 64, false}; // Unused
// Note note_5_4 = {36, 64, false}; // Unused
// Note note_5_5 = {38, 64, false}; // Unused

// // Mathew (6)
// Note note_6_0 = {64, 64, false};
// Note note_6_1 = {65, 64, false};
// Note note_6_2 = {69, 64, false};
// Note note_6_3 = {72, 64, false};
// Note note_6_4 = {76, 64, false};
// Note note_6_5 = {76, 64, false};

// // Audrey (7)
// Note note_7_0 = {69, 64, true};
// Note note_7_1 = {76, 64, true};
// Note note_7_2 = {81, 64, true};
// Note note_7_3 = {86, 64, true};
// Note note_7_4 = {81, 64, true};
// Note note_7_5 = {86, 64, true};



enum {
    OFFSET_0 = 32,
    OFFSET_1 = 0,
    OFFSET_2 = 16,
    OFFSET_3 = 0,
    OFFSET_4 = -16,
    OFFSET_5 = 32,
    OFFSET_6 = 0,
    OFFSET_7 = 0
};

// Trang
Note note_0_0 = {77, 64, false};
Note note_0_1 = {79, 64, false};
Note note_0_2 = {81, 64, false};
Note note_0_3 = {84, 64, false};
Note note_0_4 = {86, 64, false};
Note note_0_5 = {88, 64, false};

// Edwin (1)
Note note_1_0 = {65, 64, false};
Note note_1_1 = {67, 64, false};
Note note_1_2 = {69, 64, false};
Note note_1_3 = {72, 64, false};
Note note_1_4 = {74, 64, false};
Note note_1_5 = {76, 64, false};

// Victoria
Note note_2_0 = {65, 64, false};
Note note_2_1 = {67, 64, false};
Note note_2_2 = {69, 64, false};
Note note_2_3 = {72, 64, false};
Note note_2_4 = {74, 64, false};
Note note_2_5 = {76, 64, false};

// Yuktha (3)
Note note_3_0 = {64, 64, false};
Note note_3_1 = {65, 64, false};
Note note_3_2 = {69, 64, false};
Note note_3_3 = {72, 64, false};
Note note_3_4 = {74, 64, false};
Note note_3_5 = {76, 64, false};

// Jessica (4)
Note note_4_0 = {41, 80, false};
Note note_4_1 = {43, 64, false};
Note note_4_2 = {45, 64, false};
Note note_4_3 = {48, 64, false};
Note note_4_4 = {50, 64, false};
Note note_4_5 = {53, 64, false};

// Maddie
Note note_5_0 = {41, 64, false};
Note note_5_1 = {43, 64, false};
Note note_5_2 = {45, 64, false};
Note note_5_3 = {48, 64, false};
Note note_5_4 = {50, 64, false};
Note note_5_5 = {53, 64, false};

// Mathew
Note note_6_0 = {43, 64, false};
Note note_6_1 = {47, 64, false};
Note note_6_2 = {50, 64, false};
Note note_6_3 = {46, 64, false};
Note note_6_4 = {36, 64, false};
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