#include "main.h"

enum {
    SECTION_1 = 1,
    SECTION_2 = 2,
    SECTION_3 = 3
};

static const uint32_t g_section1_banks[NUM_DEVICES]    = {0, 0, 0, 0, 0, 0, 128, 128};
static const uint32_t g_section1_programs[NUM_DEVICES] = {2, 2, 0, 2, 5, 6, 0, 1};

static const uint32_t g_section2_banks[NUM_DEVICES]    = {0, 0, 128, 0, 0, 128, 0, 0};
static const uint32_t g_section2_programs[NUM_DEVICES] = {4, 104, 0, 0, 1, 16, 0, 82};

static const uint32_t g_section3_banks[NUM_DEVICES]    = {0, 0, 0, 0, 0, 0, 128, 128};
static const uint32_t g_section3_programs[NUM_DEVICES] = {2, 104, 0, 1, 2, 6, 0, 1};


/* ----- SECTION DEVICE CONFIGURATION ----- */

// Per-note config used to build runtime Note/Device objects
typedef struct {
    int pitch;
    int velocity;
    bool holdable;
} NoteConfig;

typedef struct {
    int velocity_offset;
    NoteConfig notes[SENSORS_PER_DEVICE];
} DeviceConfig;

// Runtime storage for all sections (devices[] is exposed via main.h)
static Note   g_notes[NUM_DEVICES][SENSORS_PER_DEVICE];
static Device g_devices[NUM_DEVICES];
Device* devices[NUM_DEVICES];

// ---------- Section 1: Option A (current active layout) ----------
static const DeviceConfig g_section1_devices[NUM_DEVICES] = {
    // Device 0
    { 32, {
        {65, 64, false}, {67, 64, false}, {69, 64, false},
        {72, 64, false}, {74, 64, false}, {76, 64, false}
    }},
    // Device 1
    { 32, {
        {69, 64, false}, {72, 64, false}, {74, 64, false},
        {76, 64, false}, {77, 64, false}, {81, 64, false}
    }},
    // Device 2
    { 16, {
        {65, 64, false}, {67, 64, false}, {69, 64, false},
        {72, 64, false}, {74, 64, false}, {76, 64, false}
    }},
    // Device 3
    { 0, {
        {65, 64, false}, {69, 64, false}, {72, 64, false},
        {74, 64, false}, {76, 64, false}, {77, 64, false}
    }},
    // Device 4
    { -16, {
        {41, 64, false}, {43, 64, false}, {45, 64, false},
        {48, 64, false}, {50, 64, false}, {47, 64, false}
    }},
    // Device 5
    { -16, {
        {41, 64, false}, {43, 64, false}, {45, 64, false},
        {48, 64, false}, {50, 64, false}, {53, 64, false}
    }},
    // Device 6
    { 16, {
        {36, 64, false}, {41, 64, false}, {48, 64, false},
        {45, 64, false}, {36, 64, false}, {50, 64, false}
    }},
    // Device 7
    { 0, {
        {38, 64, false}, {42, 64, false}, {46, 64, false},
        {49, 64, false}, {55, 64, false}, {38, 64, false}
    }}
};

// ---------- Section 2: Option B ----------
static const DeviceConfig g_section2_devices[NUM_DEVICES] = {
    // Device 0 (Trang)
    { 32, {
        {45, 64, true}, {48, 64, true}, {52, 64, true},
        {53, 64, true}, {56, 64, true}, {57, 64, true}
    }},
    // Device 1 (Edwin)
    { 0, {
        {64, 64, true}, {65, 64, true}, {69, 64, true},
        {72, 64, true}, {76, 64, true}, {81, 64, true}
    }},
    // Device 2 (Victoria)
    { 0, {
        {36, 64, false}, {38, 64, false}, {42, 64, false},
        {46, 64, false}, {41, 64, false}, {43, 64, false}
    }},
    // Device 3 (Yuktha)
    { 0, {
        {64, 64, false}, {65, 64, false}, {69, 64, false},
        {72, 64, false}, {74, 64, false}, {76, 64, false}
    }},
    // Device 4 (Jessica)
    { -16, {
        {45, 80, false}, {48, 64, false}, {52, 64, false},
        {45, 64, false}, {48, 64, false}, {52, 64, false}
    }},
    // Device 5 (Maddie)
    { 0, {
        {36, 80, false}, {38, 64, false}, {36, 64, false},
        {36, 64, false}, {36, 64, false}, {38, 64, false}
    }},
    // Device 6 (Mathew)
    { 32, {
        {64, 64, false}, {65, 64, false}, {69, 64, false},
        {72, 64, false}, {76, 64, false}, {76, 64, false}
    }},
    // Device 7 (Audrey)
    { 0, {
        {69, 64, true}, {76, 64, true}, {81, 64, true},
        {86, 64, true}, {81, 64, true}, {86, 64, true}
    }}
};

// ---------- Section 3: Option C ----------
static const DeviceConfig g_section3_devices[NUM_DEVICES] = {
    // Device 0 (Trang)
    { 24, {
        {77, 64, false}, {79, 64, false}, {81, 64, false},
        {84, 64, false}, {86, 64, false}, {88, 64, false}
    }},
    // Device 1 (Edwin)
    { -32, {
        {65, 64, false}, {67, 64, false}, {69, 64, false},
        {72, 64, false}, {74, 64, false}, {76, 64, false}
    }},
    // Device 2 (Victoria)
    { 24, {
        {65, 64, false}, {67, 64, false}, {69, 64, false},
        {72, 64, false}, {74, 64, false}, {76, 64, false}
    }},
    // Device 3 (Yuktha)
    { 0, {
        {64, 64, false}, {65, 64, false}, {69, 64, false},
        {72, 64, false}, {74, 64, false}, {76, 64, false}
    }},
    // Device 4 (Jessica)
    { -16, {
        {41, 80, false}, {43, 64, false}, {45, 64, false},
        {48, 64, false}, {50, 64, false}, {53, 64, false}
    }},
    // Device 5 (Maddie)
    { 16, {
        {41, 64, false}, {43, 64, false}, {45, 64, false},
        {48, 64, false}, {50, 64, false}, {53, 64, false}
    }},
    // Device 6 (Mathew)
    { 63, {
        {36, 64, false}, {41, 64, false}, {48, 64, false},
        {45, 64, false}, {36, 64, false}, {50, 64, false}
    }},
    // Device 7 (Audrey)
    { 63, {
        {38, 64, false}, {42, 64, false}, {46, 64, false},
        {49, 64, false}, {55, 64, false}, {38, 64, false}
    }}
};

static void init_devices_from_config(const DeviceConfig* cfg) {
    for (int dev_i = 0; dev_i < NUM_DEVICES; ++dev_i) {
        const DeviceConfig* dc = &cfg[dev_i];
        Device* d = &g_devices[dev_i];

        d->velocity_offset = dc->velocity_offset;
        d->old_mask = 0; // nothing pressed initially

        for (int s = 0; s < SENSORS_PER_DEVICE; ++s) {
            const NoteConfig* nc = &dc->notes[s];
            Note* n = &g_notes[dev_i][s];

            n->pitch    = nc->pitch;
            n->velocity = nc->velocity;
            n->holdable = nc->holdable;

            d->notes[s] = n;
        }

        devices[dev_i] = d;
    }
}

void init_devices_for_section(int section) {
    switch (section) {
    case SECTION_1:
        init_devices_from_config(g_section1_devices);
        break;
    case SECTION_2:
        init_devices_from_config(g_section2_devices);
        break;
    case SECTION_3:
        init_devices_from_config(g_section3_devices);
        break;
    default:
        // Fallback to section 1 if invalid
        init_devices_from_config(g_section1_devices);
        break;
    }
}





/* ----- SERIAL INPUT ----- */

static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT ||
        ctrlType == CTRL_CLOSE_EVENT) {
        InterlockedExchange(&g_running, 0);
        return TRUE;
    }
    return FALSE;
}

static HANDLE open_com_port(const char* shortName) {
    // Always use \\.\ prefix to support COM10+
    char fullName[128];
    snprintf(fullName, sizeof(fullName), "\\\\.\\%s", shortName);

    HANDLE h = CreateFileA(fullName, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[ERROR] Open %s failed (err=%lu)\n", fullName, GetLastError());
        return INVALID_HANDLE_VALUE;
    }

    // Configure buffer sizes (optional)
    SetupComm(h, 4096, 4096);

    // Serial params (adjust if your devices differ)
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(h, &dcb)) {
        fprintf(stderr, "[ERROR] GetCommState failed (err=%lu)\n", GetLastError());
        CloseHandle(h);
        return INVALID_HANDLE_VALUE;
    }

    dcb.BaudRate = CBR_115200; // match your CDC baud if relevant
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX  = FALSE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;

    if (!SetCommState(h, &dcb)) {
        fprintf(stderr, "[ERROR] SetCommState failed (err=%lu)\n", GetLastError());
        CloseHandle(h);
        return INVALID_HANDLE_VALUE;
    }

    // Timeouts: responsive but not too busy
    COMMTIMEOUTS to = {0};
    // to.ReadIntervalTimeout         = 5;    // ms between bytes
    // to.ReadTotalTimeoutConstant    = 20;   // base
    // to.ReadTotalTimeoutMultiplier  = 5;    // per byte
    to.ReadIntervalTimeout         = 1;    // ms between bytes
    to.ReadTotalTimeoutConstant    = 1;    // base
    to.ReadTotalTimeoutMultiplier  = 1;    // per byte
    to.WriteTotalTimeoutConstant   = 0;
    to.WriteTotalTimeoutMultiplier = 0;
    if (!SetCommTimeouts(h, &to)) {
        fprintf(stderr, "[ERROR] SetCommTimeouts failed (err=%lu)\n", GetLastError());
        CloseHandle(h);
        return INVALID_HANDLE_VALUE;
    }

    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return h;
}

static BOOL read_exact(HANDLE h, uint8_t* buf, DWORD len) {
    DWORD total = 0;
    while (total < len && g_running) {
        DWORD got = 0;
        if (!ReadFile(h, buf + total, len - total, &got, NULL)) {
            DWORD err = GetLastError();
            if (err == ERROR_OPERATION_ABORTED) return FALSE;
            return FALSE;
        }
        if (got == 0) {
            // Timeout, try again if still running
            continue;
        }
        total += got;
    }
    return (total == len);
}

/* Read a framed packet:
 *   FE ED <device_id> <mask>
 * Returns TRUE with payload[0]=device_id, payload[1]=mask
 */
static BOOL read_framed_packet(HANDLE h, uint8_t payload[2]) {
    enum {
        WAIT_SYNC_FE = 0,
        WAIT_SYNC_ED = 1
    } state = WAIT_SYNC_FE;

    uint8_t b = 0;
    while (g_running) {
        DWORD got = 0;
        if (!ReadFile(h, &b, 1, &got, NULL)) {
            DWORD err = GetLastError();
            if (err == ERROR_OPERATION_ABORTED) {
                return FALSE;
            }
            // treat other errors as non-fatal; try again
            continue;
        }
        if (got == 0) {
            // timeout, keep looping while running
            continue;
        }

        if (state == WAIT_SYNC_FE) {
            if (b == 0xFE) {
                state = WAIT_SYNC_ED;
            }
        } else { // WAIT_SYNC_ED
            if (b == 0xED) {
                // Got full sync FE ED, now read the 2-byte payload
                if (!read_exact(h, payload, 2)) {
                    return FALSE;
                }
                return TRUE;
            } else if (b == 0xFE) {
                // Could be start of new sync sequence (FE FE ED...)
                state = WAIT_SYNC_ED;
            } else {
                // Lost sync, go back to looking for FE
                state = WAIT_SYNC_FE;
            }
        }
    }
    return FALSE;
}

/* ---------- FluidSynth helpers ---------- */

static void fs_note_on(int chan, int key, int vel) {
    EnterCriticalSection(&g_synthLock);
    fluid_synth_noteon(g_synth, chan, key, vel);
    LeaveCriticalSection(&g_synthLock);
}

static void fs_note_off(int chan, int key) {
    EnterCriticalSection(&g_synthLock);
    fluid_synth_noteoff(g_synth, chan, key);
    LeaveCriticalSection(&g_synthLock);
}

static int fs_load_for_channel(int chan) {
    // returns 0 on success, -1 on failure
    int id = fluid_synth_sfload(g_synth, g_chan[chan].sf2_path, 1 /*reset presets*/);
    if (id < 0) {
        fprintf(stderr, "[ERROR] sfload failed for channel %d: %s\n", chan, g_chan[chan].sf2_path);
        return -1;
    }
    g_chan[chan].sfont_id = id;

    // Select the sfont (and program/bank) for this MIDI channel
    if (fluid_synth_program_select(g_synth, chan, id, g_chan[chan].bank, g_chan[chan].program) != FLUID_OK) {
        fprintf(stderr, "[ERROR] program_select failed on channel %d\n", chan);
        return -1;
    }
    return 0;
}

/* ---------- Init / teardown ---------- */

static int fluidsynth_init(const char* preferred_driver) {
    InitializeCriticalSection(&g_synthLock);

    g_settings = new_fluid_settings();
    if (!g_settings) {
        fprintf(stderr, "[ERROR] new_fluid_settings failed\n");
        return -1;
    }

    // Audio driver
    fluid_settings_setstr(g_settings, "audio.driver", preferred_driver && preferred_driver[0] ? preferred_driver : "wasapi");

    // Reasonable defaults; adjust for your latency/CPU balance
    fluid_settings_setnum(g_settings, "synth.gain", 6.0);
    fluid_settings_setnum(g_settings, "synth.sample-rate", 48000.0);
    fluid_settings_setint(g_settings, "audio.periods", 2);
    fluid_settings_setint(g_settings, "synth.cpu-cores", 1);

    fluid_settings_setstr(g_settings, "audio.wasapi.device", "default");
    fluid_settings_setint(g_settings, "audio.period-size", 64);
    fluid_settings_setint(g_settings, "audio.wasapi.exclusive-mode", 0);


    fluid_settings_setint(g_settings, "synth.min-note-length", 125);
    fluid_settings_setnum(g_settings, "synth.reverb.room-size", 1.0);
    fluid_settings_setnum(g_settings, "synth.reverb.damp",    1.0);
    fluid_settings_setnum(g_settings, "synth.reverb.width",   0.8);
    fluid_settings_setnum(g_settings, "synth.reverb.level",   1.0);
    fluid_settings_setint(g_settings, "synth.reverb.active",  1);

    g_synth = new_fluid_synth(g_settings);
    if (!g_synth) {
        fprintf(stderr, "[ERROR] new_fluid_synth failed\n");
        return -1;
    }

    g_adriver = new_fluid_audio_driver(g_settings, g_synth);
    if (!g_adriver) {
        fprintf(stderr, "[ERROR] new_fluid_audio_driver failed\n");
        return -1;
    }

    // Load/select soundfont per channel
    for (int ch = 0; ch < NUM_DEVICES; ++ch) {
        if (fs_load_for_channel(ch) < 0) return -1;
    }
    return 0;
}

static void fluidsynth_shutdown(void) {
    if (g_adriver) { delete_fluid_audio_driver(g_adriver); g_adriver = NULL; }
    if (g_synth)   { delete_fluid_synth(g_synth); g_synth = NULL; }
    if (g_settings){ delete_fluid_settings(g_settings); g_settings = NULL; }
    DeleteCriticalSection(&g_synthLock);
}

/* ---------- CLI parsing ---------- */

static bool starts_with(const char* s, const char* pfx) {
    size_t n = strlen(pfx);
    return strncmp(s, pfx, n) == 0;
}

static void usage(const char* exe) {
    fprintf(stderr,
        "Usage:\n"
        "  %s [COMx] [--sf2=<path>] [--driver=wasapi|dsound|winmm] [--section=1|2|3]\n"
        "\n"
        "Examples:\n"
        "  %s COM5 --sf2=usb.sf2 --section=1\n"
        "  %s --sf2=usb.sf2 --section=2      (defaults to COM1)\n"
        "\n"
        "Notes:\n"
        " - device_id (0..7) is used as MIDI channel\n"
        " - --sf2 sets a single soundfont for all channels\n"
        " - --section selects which bank/program configuration to use\n",
        exe, exe, exe);
}

/* ---------- Main: single COM, single device stream ---------- */

int main(int argc, char** argv) {
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    // or REALTIME_PRIORITY_CLASS if this is a dedicated machine and you know the risks


    int section = SECTION_1;

    // Default to a single COM port
    char com_port[64] = "COM5";

    // Soundfont defaults (same sf2 for all channels)
    for (int i = 0; i < NUM_DEVICES; ++i) {
        snprintf(g_chan[i].sf2_path, sizeof(g_chan[i].sf2_path), "usb.sf2");
        g_chan[i].bank = 0;
        g_chan[i].program = 0;
        g_chan[i].sfont_id = -1;
    }

    char driver_opt[32] = "wasapi";

    // Parse args: COM, --sf2, --driver, --section
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];

        if (starts_with(a, "--sf2=")) {
            const char* path = a + 6;
            for (int c = 0; c < NUM_DEVICES; ++c) {
                snprintf(g_chan[c].sf2_path, sizeof(g_chan[c].sf2_path), "%s", path);
            }
        } else if (starts_with(a, "--driver=")) {
            snprintf(driver_opt, sizeof(driver_opt), "%s", a + 9); // wasapi|dsound|winmm
        } else if (starts_with(a, "--section=")) {
            const char* val = a + 10;
            section = atoi(val);
            if (section < SECTION_1 || section > SECTION_3) {
                fprintf(stderr, "[ERROR] Invalid section '%s'. Must be 1, 2, or 3.\n", val);
                return 1;
            }
        } else if (starts_with(a, "COM")) {
            snprintf(com_port, sizeof(com_port), "%s", a);
        } else {
            usage(argv[0]);
            return 1;
        }
    }


    
    const uint32_t* banks = NULL;
    const uint32_t* programs = NULL;

    switch (section) {
    case SECTION_1:
        banks    = g_section1_banks;
        programs = g_section1_programs;
        break;
    case SECTION_2:
        banks    = g_section2_banks;
        programs = g_section2_programs;
        break;
    case SECTION_3:
        banks    = g_section3_banks;
        programs = g_section3_programs;
        break;
    default:
        fprintf(stderr, "[ERROR] Unknown section %d\n", section);
        return 1;
    }

    for (int i = 0; i < NUM_DEVICES; i++) {
        g_chan[i].program = programs[i];
        g_chan[i].bank    = banks[i];
    }

    // Initialize per-device note/velocity layout for this section
    init_devices_for_section(section);

    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    // Init FluidSynth
    if (fluidsynth_init(driver_opt) < 0) {
        fprintf(stderr, "[FATAL] FluidSynth init failed\n");
        fluidsynth_shutdown();
        return 1;
    }

    // Open single COM port
    HANDLE h = open_com_port(com_port);
    if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[FATAL] Could not open %s\n", com_port);
        fluidsynth_shutdown();
        return 1;
    }

    fprintf(stdout, "[INFO] Listening on %s\n", com_port);
    fprintf(stdout, "[INFO] Running. Press Ctrl+C to exit.\n");

    uint8_t pkt[2]; // payload: [0]=device_id, [1]=mask

    while (g_running) {
        // Read framed packet: FE ED <device_id> <mask>
        if (!read_framed_packet(h, pkt)) {
            // likely a timeout or transient error — keep going while running
            continue;
        }

        int device_id = pkt[0];
        uint8_t new_mask = pkt[1];

        if (device_id < 0 || device_id >= NUM_DEVICES) {
            // invalid packet, ignore
            continue;
        }

        int midi_chan = device_id;
        Device* dev = devices[device_id];

        uint8_t diff = new_mask ^ dev->old_mask;
        for (uint8_t sensor_id = 0; sensor_id < SENSORS_PER_DEVICE; sensor_id++) {
            uint8_t pad_mask = (uint8_t)(1U << sensor_id);
            if (diff & pad_mask) {
                bool state_on = (new_mask & pad_mask) ? true : false;
                Note* note = dev->notes[sensor_id];
                if (state_on) {
                    // note on
                    fs_note_on(midi_chan, note->pitch, note->velocity + dev->velocity_offset);
                } else {
                    // note off
                    if (note->holdable) {
                        fs_note_off(midi_chan, note->pitch);
                    }
                }
            }
        }
        dev->old_mask = new_mask;
    }

    CloseHandle(h);
    fluidsynth_shutdown();
    fprintf(stdout, "[INFO] Clean exit.\n");
    return 0;
}
