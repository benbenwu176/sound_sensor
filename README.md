# Sound Sensor
Wireless touch sensor soundboard project for the Society of Unconventional Drummers

# How to run:

## Software:

* Install fluidsynth from https://github.com/FluidSynth/fluidsynth/releases/tag/v2.4.6
* Extract, set PATH to Program Files/FluidSynth/bin
* Make sure you have python/pip installed and run `pip install pyfluidsynth`
* (crappy patch) Copy fluidsynth bin file to C:/tools/fluidsynth/bin

* Run `pip install pyserial`
* Update COM path



git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg install fluidsynth:x64-windows-static
cd path\to\serial_synth
Install CMake MSI at https://cmake.org/download/
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

:: Easiest: default COM1..COM8, all channels use usb.sf2
.\build\Release\serial_synth.exe --sf2=usb.sf2

:: Explicit COM ports (must match your system), still all use usb.sf2
.\build\Release\serial_synth.exe COM3 COM4 COM5 COM6 COM7 COM8 COM9 COM10 --sf2=usb.sf2

:: Different soundfonts per device/channel (0..7), and custom programs
.\build\Release\serial_synth.exe COM3 COM4 COM5 COM6 COM7 COM8 COM9 COM10 ^
  --sf20=marimba.sf2 --prog0=12 ^
  --sf23=piano.sf2   --prog3=0  ^
  --sf2=usb.sf2      --driver=wasapi

* Install vcpkg
* Run `vcpkg install fluidsynth`

gcc play_note.c -o play_note $(pkg-config --cflags --libs fluidsynth)
./play_note /path/to/your.sf2 0



## Embedded:

* Install VCP drivers
* If flashing install ST-Link v2 drivers & update if needed