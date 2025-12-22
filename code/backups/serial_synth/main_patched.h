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

// Runtime device map (one Device* per logical device, indexed by device_id 0..7)
extern Device* devices[NUM_DEVICES];

// Initialize the global 'devices' array for the given musical section (1, 2, or 3)
void init_devices_for_section(int section);

