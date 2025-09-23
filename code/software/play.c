// play_note.c
#include <stdio.h>

#ifdef _WIN32
  #include <windows.h>
  #define msleep(ms) Sleep(ms)
#else
  #include <unistd.h>
  #define msleep(ms) usleep((ms) * 1000)
#endif

int main(void) {
    const char *sf2_path = "usb.sf2";   // put usb.sf2 next to the binary or use an absolute path
    const int chan = 0;                 // MIDI channel 0
    const int key  = 60;                // Middle C
    const int vel  = 100;               // Velocity 1..127
    const int hold_ms = 2000;           // How long to hold the note

    fluid_settings_t *settings = new_fluid_settings();
    if (!settings) { fprintf(stderr, "Failed to create FluidSynth settings\n"); return 1; }

    // (Optional) pick an audio driver if you know what you want:
    // fluid_settings_setstr(settings, "audio.driver", "pulseaudio");   // or "alsa", "coreaudio", "dsound", etc.

    fluid_synth_t *synth = new_fluid_synth(settings);
    if (!synth) { fprintf(stderr, "Failed to create FluidSynth synth\n"); delete_fluid_settings(settings); return 1; }

    fluid_audio_driver_t *adriver = new_fluid_audio_driver(settings, synth);
    if (!adriver) {
        fprintf(stderr, "Failed to create FluidSynth audio driver\n");
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
        return 1;
    }

    int sfont_id = fluid_synth_sfload(synth, sf2_path, /*reset_presets=*/1);
    if (sfont_id == FLUID_FAILED) {
        fprintf(stderr, "Could not load soundfont: %s\n", sf2_path);
        delete_fluid_audio_driver(adriver);
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
        return 1;
    }

    // Select bank 0, program 0 on channel 0. Change these if your SF2 organizes presets differently.
    if (fluid_synth_program_select(synth, chan, sfont_id, /*bank=*/0, /*program=*/0) == FLUID_FAILED) {
        fprintf(stderr, "program_select failed (bank 0, program 0). Try different bank/program.\n");
    }

    // Note on -> wait -> note off
    if (fluid_synth_noteon(synth, chan, key, vel) == FLUID_FAILED) {
        fprintf(stderr, "noteon failed\n");
    }
    msleep(hold_ms);
    if (fluid_synth_noteoff(synth, chan, key) == FLUID_FAILED) {
        fprintf(stderr, "noteoff failed\n");
    }

    // A brief pause to let the tail finish on some drivers
    msleep(250);

    delete_fluid_audio_driver(adriver);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return 0;
}
