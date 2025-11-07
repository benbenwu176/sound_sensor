#include "main.h"

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

    dcb.BaudRate = CBR_115200; // common default
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
    to.ReadIntervalTimeout         = 5;    // ms between bytes
    to.ReadTotalTimeoutConstant    = 20;   // base
    to.ReadTotalTimeoutMultiplier  = 5;    // per byte
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
            // non-fatal: break if no data and timeouts
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

// -------------- FluidSynth helpers ----------------

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
    // (Either program_select or sfont_select+program_change; this does all in one)
    if (fluid_synth_program_select(g_synth, chan, id, g_chan[chan].bank, g_chan[chan].program) != FLUID_OK) {
        fprintf(stderr, "[ERROR] program_select failed on channel %d\n", chan);
        return -1;
    }
    return 0;
}

// -------------- Serial thread ----------------

static DWORD WINAPI serial_thread_proc(LPVOID param) {
    SerialThreadArgs* args = (SerialThreadArgs*)param;
    HANDLE h = open_com_port(args->port_name);
    if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[WARN] Thread for %s exiting (could not open)\n", args->port_name);
        return 1;
    }

    uint8_t pkt[3];
    while (g_running) {
        if (!read_exact(h, pkt, 2)) {
            // likely a timeout — just continue while running
            continue;
        }
        int device_id = pkt[0];
        int midi_chan = device_id;
        Device* dev = devices[device_id];
        uint8_t new_mask = pkt[1];
        uint8_t diff = new_mask ^ dev->old_mask;
        for (uint8_t sensor_id = 0; sensor_id < SENSORS_PER_DEVICE; sensor_id++) {
            uint8_t pad_mask = diff & (1U << sensor_id);
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
    return 0;
}

// -------------- Init / teardown ----------------

static int fluidsynth_init(const char* preferred_driver) {
    InitializeCriticalSection(&g_synthLock);

    g_settings = new_fluid_settings();
    if (!g_settings) {
        fprintf(stderr, "[ERROR] new_fluid_settings failed\n");
        return -1;
    }

    // Audio driver
    if (preferred_driver && preferred_driver[0]) {
        fluid_settings_setstr(g_settings, "audio.driver", preferred_driver); // "wasapi" recommended on modern Windows
    } else {
        fluid_settings_setstr(g_settings, "audio.driver", "wasapi");
    }

    // Reasonable defaults; adjust for your latency/CPU balance
    fluid_settings_setnum(g_settings, "synth.gain", 0.8);
    fluid_settings_setnum(g_settings, "synth.sample-rate", 48000.0);
    fluid_settings_setint(g_settings, "audio.periods", 2);
    fluid_settings_setint(g_settings, "audio.period-size", 128);

    // fluid_settings_setint(g_settings, "synth.cpu-cores", 8); // uncomment when wireless

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

// -------------- CLI parsing ----------------

static bool starts_with(const char* s, const char* pfx) {
    size_t n = strlen(pfx);
    return strncmp(s, pfx, n) == 0;
}

static void usage(const char* exe) {
    fprintf(stderr,
        "Usage:\n"
        "  %s [COM ports...] [--sf2=<path>] [--sf20=<path> ... --sf27=<path>] [--bankN=<b>] [--progN=<p>] [--driver=wasapi|dsound|winmm]\n"
        "\n"
        "Examples:\n"
        "  %s COM3 COM4 COM5 COM6 COM7 COM8 COM9 COM10 --sf2=usb.sf2\n"
        "  %s --sf2=usb.sf2             (defaults to COM1..COM8)\n"
        "  %s COM11 COM12 ... COM18 --sf20=marimba.sf2 --prog0=12 --sf23=piano.sf2 --prog3=0\n"
        "\n"
        "Notes:\n"
        " - device_id is used as MIDI channel (0..7)\n"
        " - per-channel sf2 overrides use flags --sf2N (N=0..7); otherwise --sf2 is used for all\n"
        " - default bank=0, program=0 per channel unless overridden with --bankN / --progN\n"
        , exe, exe, exe, exe);
}

int main(int argc, char** argv) {
    // Defaults
    char com_list[MAX_COM_PORTS][64];
    int  com_count = 0;

    // Soundfont defaults
    for (int i = 0; i < NUM_DEVICES; ++i) {
        snprintf(g_chan[i].sf2_path, sizeof(g_chan[i].sf2_path), "usb.sf2");
        g_chan[i].bank = 0;
        g_chan[i].program = 0;
        g_chan[i].sfont_id = -1;
    }

    char driver_opt[32] = "wasapi";

    // Parse args
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];

        if (starts_with(a, "--sf2=")) {
            const char* path = a + 6;
            for (int c = 0; c < NUM_DEVICES; ++c) {
                snprintf(g_chan[c].sf2_path, sizeof(g_chan[c].sf2_path), "%s", path);
            }
        } else if (starts_with(a, "--sf2")) {
            // per-channel: --sf2N=...
            int idx = a[5] - '0'; // a like "--sf2N=..."
            const char* eq = strchr(a, '=');
            if (idx >= 0 && idx < NUM_DEVICES && eq) {
                snprintf(g_chan[idx].sf2_path, sizeof(g_chan[idx].sf2_path), "%s", eq + 1);
            } else {
                usage(argv[0]); return 1;
            }
        } else if (starts_with(a, "--prog")) {
            // --progN=#
            int idx = a[6] - '0';
            const char* eq = strchr(a, '=');
            if (idx >= 0 && idx < NUM_DEVICES && eq) {
                g_chan[idx].program = atoi(eq + 1);
                if (g_chan[idx].program < 0) g_chan[idx].program = 0;
                if (g_chan[idx].program > 127) g_chan[idx].program = 127;
            } else {
                usage(argv[0]); return 1;
            }
        } else if (starts_with(a, "--bank")) {
            // --bankN=#
            int idx = a[6] - '0';
            const char* eq = strchr(a, '=');
            if (idx >= 0 && idx < NUM_DEVICES && eq) {
                g_chan[idx].bank = atoi(eq + 1);
                if (g_chan[idx].bank < 0) g_chan[idx].bank = 0;
            } else {
                usage(argv[0]); return 1;
            }
        } else if (starts_with(a, "--driver=")) {
            snprintf(driver_opt, sizeof(driver_opt), "%s", a + 9); // wasapi|dsound|winmm
        } else if (starts_with(a, "COM")) {
            if (com_count < MAX_COM_PORTS) {
                snprintf(com_list[com_count++], sizeof(com_list[0]), "%s", a);
            }
        } else {
            usage(argv[0]); return 1;
        }
    }
    // uint32_t banks[] = {0, 0, 0, 0, 0, 0, 128, 128};
    // uint32_t programs[] = {2, 2, 0, 2, 5, 6, 0, 1};
    uint32_t banks[] = {0, 0, 128, 0, 0, 128, 0, 0};
    uint32_t programs[] = {4, 1, 0, 0, 1, 16, 0, 82};
    
    for (int i = 0; i < NUM_DEVICES; i++) {
        g_chan[i].program = programs[i];
        g_chan[i].bank = banks[i];
    }

    if (com_count == 0) {
        // Default to COM1..COM8
        for (int i = 0; i < NUM_DEVICES; ++i) {
            snprintf(com_list[com_count++], sizeof(com_list[0]), "COM%d", i + 1);
        }
    } else if (com_count != NUM_DEVICES) {
        fprintf(stderr, "[WARN] You provided %d COM ports; device_id expects 8. Proceeding anyway.\n", com_count);
    }

    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    // Init FluidSynth
    if (fluidsynth_init(driver_opt) < 0) {
        fprintf(stderr, "[FATAL] FluidSynth init failed\n");
        fluidsynth_shutdown();
        return 1;
    }

    // Start serial threads
    HANDLE threads[MAX_COM_PORTS] = {0};
    SerialThreadArgs* args[MAX_COM_PORTS] = {0};

    for (int i = 0; i < com_count; ++i) {
        args[i] = (SerialThreadArgs*)calloc(1, sizeof(SerialThreadArgs));
        snprintf(args[i]->port_name, sizeof(args[i]->port_name), "%s", com_list[i]);
        threads[i] = CreateThread(NULL, 0, serial_thread_proc, args[i], 0, NULL);
        if (!threads[i]) {
            fprintf(stderr, "[ERROR] Could not start thread for %s\n", com_list[i]);
        } else {
            fprintf(stdout, "[INFO] Listening on %s\n", com_list[i]);
        }
    }

    fprintf(stdout, "[INFO] Running. Press Ctrl+C to exit.\n");

    // Wait until Ctrl+C
    while (g_running) {
        Sleep(100);
    }

    // Join threads
    for (int i = 0; i < com_count; ++i) {
        if (threads[i]) {
            WaitForSingleObject(threads[i], 2000);
            CloseHandle(threads[i]);
        }
        if (args[i]) free(args[i]);
    }

    fluidsynth_shutdown();
    fprintf(stdout, "[INFO] Clean exit.\n");
    return 0;
}
