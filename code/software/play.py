import time
import serial
import fluidsynth



# Init FluidSynth sampler
fs = fluidsynth.Synth(samplerate=48000, gain=0.5)
fs.start()
sf_id = fs.sfload("usb.sf2")
fs.program_select(0, sf_id, 0, 0)
input = serial.Serial("COM5", 115200, timeout=0.1)

# Play test chords
def play_test():
    vol = 127
    chord = [60, 64, 67]

    for i in range(10):
        for note in chord:
            fs.noteon(0, note, vol)
        time.sleep(0.10)

    time.sleep(5.0)    

# Main receiver loop
def main():
    buf = bytearray()
    while True:
        buf.extend(input.read(64))
        # Consume complete 3-byte MIDI messages (no running status)
        while len(buf) >= 3:
            header, midi, vel = buf[0], buf[1], buf[2]
            # TODO: parse into notes
            del buf[:3]
            header, midi, vel = 0x10, 60, 127
            cmd = (header & 0xF0) >> 4 # First hex char of header = on or off
            channel = header & 0x0F # Second hex char of header = channel val
            
            # TODO: what is channel? also, worry about response with time.sleep
            if (cmd == 1): # Note on
                fs.noteon(channel, midi, vel)
            elif (cmd == 0):
                fs.noteoff(channel, midi)
            else:
                print("Unknown command: " + cmd)
            
# Run main loop
main()

# Delete after done
fs.delete()