# Sound Sensor
This is a project/ensemble for the Texas Society of Unconventional Drummers (Texas SOUnD).

The primary focus of this project is touch sensor to sound output, which is accomplished through
capacitive touch sensors, wireless communication to a receiving hub, and FluidSynth soundfont processing.

# How to run:

## Software:

### Installation (May take a while for the FluidSynth static library):

* Install CMake MSI at https://cmake.org/download/
* Run the following commands to install fluidsynth:
    * `git clone https://github.com/microsoft/vcpkg C:\vcpkg`
    * `C:\vcpkg\bootstrap-vcpkg.bat`
    * `C:\vcpkg\vcpkg install fluidsynth:x64-windows-static`
* Install a soundfont file to be played

### Build/Run:

* To build: 
    * cd to your project directory
    * Build the toolchain:
        * `cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -DCMAKE_BUILD_TYPE=Release`
    * Build the program:
        * `cmake --build build --config Release`    

* To run:
    * Easiest, default COM1-COM8, all channels use usb.sf2
        * `.\build\Release\serial_synth.exe --sf2=usb.sf2`
    * Explicit COM ports (must match your system), still all use usb.sf2
        * `.\build\Release\serial_synth.exe COM3 COM4 COM5 COM6 COM7 COM8 COM9 COM10 --sf2=usb.sf2`
    * Different soundfonts per device/channel (0..7), and custom programs
        * `.\build\Release\serial_synth.exe COM3 COM4 COM5 COM6 COM7 COM8 COM9 COM10 --sf20=marimba.sf2 --prog0=12 --sf23=piano.sf2 --prog3=0 --sf2=usb.sf2 --driver=wasapi`
    * Production: 
        * `.\build\Release\serial_synth.exe COM6 COM7 COM8 COM9 COM10 COM11 COM12 --sf2=basic.sf2 --bank0=0 --prog0=61 --bank1=0 --prog1=61 --bank2=0 --prog2=0  --bank3=0 --prog3=0  --bank4=0 --prog4=0 --bank5=0 --prog5=32 --bank6=128 --prog6=0 --bank7=128 --prog7=0`

* Example when a simple change is made (rebuild, run):
    * `cmake --build build --config Release`
    * `.\build\Release\serial_synth.exe --sf2=usb.sf2`

## Embedded:

* Install VCP drivers
* If flashing install ST-Link v2 drivers & update if needed

# Resources: 

* Fluid synth driver settings: https://www.fluidsynth.org/api/settings_synth.html
* MuseScore basic soundfont (all instruments): https://musical-artifacts.com/artifacts/3001
* Black pill JTAG pinout: https://developer.arm.com/documentation/100893/1-0/Target-interface-connectors/Arm-JTAG-20-connector?
* nRF24L01+ pinout: https://lastminuteengineers.com/nrf24l01-arduino-wireless-communication/
* MPR121 guide: https://youtu.be/tTMsbL0eH_M?si=9AUp4mB7ZUm1yg_a
* USB VCOM setup: https://www.youtube.com/watch?v=92A98iEFmaA
* ST-Link connection: https://www.youtube.com/watch?v=QRW2tZ1pNR8
* Repo link: https://github.com/benbenwu176/sound_sensor.git