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
    if (preferred_driver && preferred_driver[0]) {
        fluid_settings_setstr(g_settings, "audio.driver", preferred_driver); // "wasapi" recommended on modern Windows
    } else {
        fluid_settings_setstr(g_settings, "audio.driver", "wasapi");
    }

    // Reasonable defaults; adjust for your latency/CPU balance
    fluid_settings_setnum(g_settings, "synth.gain", 6.0);
    fluid_settings_setnum(g_settings, "synth.sample-rate", 48000.0);
    fluid_settings_setint(g_settings, "audio.periods", 2);
    fluid_settings_setint(g_settings, "audio.period-size", 128);
    fluid_settings_setint(g_settings, "synth.cpu-cores", 4);


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
        "  %s [COMx] [--sf2=<path>] [--driver=wasapi|dsound|winmm]\n"
        "\n"
        "Examples:\n"
        "  %s COM5 --sf2=usb.sf2\n"
        "  %s --sf2=usb.sf2             (defaults to COM1)\n"
        "\n"
        "Notes:\n"
        " - device_id (0..7) is used as MIDI channel\n"
        " - --sf2 sets a single soundfont for all channels\n",
        exe, exe, exe);
}

/* ---------- Main: single COM, single device stream ---------- */

int main(int argc, char** argv) {
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

    // Parse args: COM, --sf2, --driver only
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];

        if (starts_with(a, "--sf2=")) {
            const char* path = a + 6;
            for (int c = 0; c < NUM_DEVICES; ++c) {
                snprintf(g_chan[c].sf2_path, sizeof(g_chan[c].sf2_path), "%s", path);
            }
        } else if (starts_with(a, "--driver=")) {
            snprintf(driver_opt, sizeof(driver_opt), "%s", a + 9); // wasapi|dsound|winmm
        } else if (starts_with(a, "COM")) {
            snprintf(com_port, sizeof(com_port), "%s", a);
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    // Optional: baked-in banks/programs per logical device

    uint32_t banks[] = {0, 0, 0, 0, 0, 0, 128, 128};
    uint32_t programs[] = {2, 2, 0, 2, 5, 6, 0, 1};

    // uint32_t banks[] = {0, 0, 128, 0, 0, 128, 0, 0};
    // uint32_t programs[] = {4, 104, 0, 0, 1, 16, 0, 82};

    // uint32_t banks[] = {0, 0, 0, 0, 0, 0, 128, 128};
    // uint32_t programs[] = {2, 104, 0, 1, 2, 6, 0, 1};

    for (int i = 0; i < NUM_DEVICES; i++) {
        g_chan[i].program = programs[i];
        g_chan[i].bank = banks[i];
    }

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
