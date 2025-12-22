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

/* ----- DEVICE CONFIGURATIONS (runtime-selectable) ----- */

typedef struct {
    int pitch;
    int velocity;
    bool holdable;
} Note;

typedef struct {
    const Note* notes[SENSORS_PER_DEVICE];
    int velocity_offset;
    uint8_t old_mask;
} Device;

typedef struct {
    int offsets[NUM_DEVICES];
    Note notes[NUM_DEVICES][SENSORS_PER_DEVICE];
    uint32_t banks[NUM_DEVICES];
    uint32_t programs[NUM_DEVICES];
} SectionConfig;


/* ---------- Section 1 ---------- */
static const SectionConfig Section_1 = {
        { 32, 32, 16, 0, -16, -16, 16, 0 },
        {
        { { { 65, 64, false }, { 67, 64, false }, { 69, 64, false }, { 72, 64, false }, { 74, 64, false }, { 76, 64, false } } }, // 0
        { { { 69, 64, false }, { 72, 64, false }, { 74, 64, false }, { 76, 64, false }, { 77, 64, false }, { 81, 64, false } } }, // 1
        { { { 65, 64, false }, { 67, 64, false }, { 69, 64, false }, { 72, 64, false }, { 74, 64, false }, { 76, 64, false } } }, // 2
        { { { 65, 64, false }, { 69, 64, false }, { 72, 64, false }, { 74, 64, false }, { 76, 64, false }, { 77, 64, false } } }, // 3
        { { { 41, 64, false }, { 43, 64, false }, { 45, 64, false }, { 48, 64, false }, { 50, 64, false }, { 47, 64, false } } }, // 4
        { { { 41, 64, false }, { 43, 64, false }, { 45, 64, false }, { 48, 64, false }, { 50, 64, false }, { 53, 64, false } } }, // 5
        { { { 36, 64, false }, { 41, 64, false }, { 48, 64, false }, { 45, 64, false }, { 36, 64, false }, { 50, 64, false } } }, // 6
        { { { 38, 64, false }, { 42, 64, false }, { 46, 64, false }, { 49, 64, false }, { 55, 64, false }, { 38, 64, false } } }, // 7
    },
        { 0, 0, 0, 0, 0, 0, 128, 128 },
        { 2, 2, 0, 2, 5, 6, 0, 1 }
};

/* ---------- Section 2 ---------- */
static const SectionConfig Section_2 = {
        { 32, 0, 0, 0, -16, 0, 32, 0 },
        {
        { { { 45, 64, true }, { 48, 64, true }, { 52, 64, true }, { 53, 64, true }, { 56, 64, true }, { 57, 64, true } } }, // 0
        { { { 64, 64, true }, { 65, 64, true }, { 69, 64, true }, { 72, 64, true }, { 76, 64, true }, { 81, 64, true } } }, // 1
        { { { 36, 64, false }, { 38, 64, false }, { 42, 64, false }, { 46, 64, false }, { 41, 64, false }, { 43, 64, false } } }, // 2
        { { { 64, 64, false }, { 65, 64, false }, { 69, 64, false }, { 72, 64, false }, { 74, 64, false }, { 76, 64, false } } }, // 3
        { { { 45, 80, false }, { 48, 64, false }, { 52, 64, false }, { 45, 64, false }, { 48, 64, false }, { 52, 64, false } } }, // 4
        { { { 36, 80, false }, { 38, 64, false }, { 36, 64, false }, { 36, 64, false }, { 36, 64, false }, { 38, 64, false } } }, // 5
        { { { 64, 64, false }, { 65, 64, false }, { 69, 64, false }, { 72, 64, false }, { 76, 64, false }, { 76, 64, false } } }, // 6
        { { { 69, 64, true }, { 76, 64, true }, { 81, 64, true }, { 86, 64, true }, { 81, 64, true }, { 86, 64, true } } }, // 7
    },
        { 0, 0, 128, 0, 0, 128, 0, 0 },
        { 4, 104, 0, 0, 1, 16, 0, 82 }
};

/* ---------- Section 3 ---------- */
static const SectionConfig Section_3 = {
        { 24, -32, 24, 0, -16, 16, 63, 63 },
        {
        { { { 77, 64, false }, { 79, 64, false }, { 81, 64, false }, { 84, 64, false }, { 86, 64, false }, { 88, 64, false } } }, // 0
        { { { 65, 64, false }, { 67, 64, false }, { 69, 64, false }, { 72, 64, false }, { 74, 64, false }, { 76, 64, false } } }, // 1
        { { { 65, 64, false }, { 67, 64, false }, { 69, 64, false }, { 72, 64, false }, { 74, 64, false }, { 76, 64, false } } }, // 2
        { { { 64, 64, false }, { 65, 64, false }, { 69, 64, false }, { 72, 64, false }, { 74, 64, false }, { 76, 64, false } } }, // 3
        { { { 41, 80, false }, { 43, 64, false }, { 45, 64, false }, { 48, 64, false }, { 50, 64, false }, { 53, 64, false } } }, // 4
        { { { 41, 64, false }, { 43, 64, false }, { 45, 64, false }, { 48, 64, false }, { 50, 64, false }, { 53, 64, false } } }, // 5
        { { { 36, 64, false }, { 41, 64, false }, { 48, 64, false }, { 45, 64, false }, { 36, 64, false }, { 50, 64, false } } }, // 6
        { { { 38, 64, false }, { 42, 64, false }, { 46, 64, false }, { 49, 64, false }, { 55, 64, false }, { 38, 64, false } } }, // 7
    },
        { 0, 0, 0, 0, 0, 0, 128, 128 },
        { 2, 104, 0, 1, 2, 6, 0, 1 }
};

/* devices[] will be built at runtime in main.c */
extern Device* devices[NUM_DEVICES];
