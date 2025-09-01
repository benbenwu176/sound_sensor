import time
import fluidsynth

fs = fluidsynth.Synth(samplerate=48000, gain=0.5)
fs.start(driver="wasapi")

sfid = fs.sfload("usb.sf2")
fs.program_select(0, sfid, 0, 0)

vol = 127
chord = [60, 64, 67]

for i in range(10):
    for note in chord:
        fs.noteon(0, note, vol)
    time.sleep(0.5)

time.sleep(5.0)

fs.delete()