cmake_minimum_required(VERSION 3.20)
project(serial_synth C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Point to vcpkg toolchain when you configure (see build commands below)
find_package(FluidSynth CONFIG REQUIRED)

add_executable(serial_synth main.c)

# For Windows 7+ APIs and WASAPI
if (MSVC)
  target_compile_definitions(serial_synth PRIVATE _WIN32_WINNT=0x0601)
endif()

target_link_libraries(serial_synth PRIVATE FluidSynth::libfluidsynth)

# Strict-ish warnings
if (MSVC)
  target_compile_options(serial_synth PRIVATE /W4 /permissive-)
else()
  target_compile_options(serial_synth PRIVATE -Wall -Wextra -Wpedantic)
endif()
