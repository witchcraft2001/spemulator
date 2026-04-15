cmake_minimum_required(VERSION 3.16)
project(spemulator C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG -DSPEMU_TRACE")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

# Option to enable trace in release builds
option(SPEMU_ENABLE_TRACE "Enable trace logging in release builds" OFF)
if(SPEMU_ENABLE_TRACE)
    add_definitions(-DSPEMU_TRACE)
endif()

# Find SDL3
find_package(SDL3 REQUIRED)

# Platform detection
if(APPLE)
    add_definitions(-DPLATFORM_MACOS)
elseif(WIN32)
    add_definitions(-DPLATFORM_WINDOWS)
else()
    add_definitions(-DPLATFORM_LINUX)
endif()

# Source files
set(SOURCES
    src/main.c
    src/config.c
    src/machine.c
    src/bus.c
    src/cpu/z80.c
    src/cpu/z80_ops.c
    src/cpu/z80_cb.c
    src/cpu/z80_ed.c
    src/cpu/z80_dd.c
    src/cpu/z80_ddcb.c
    src/cpu/z80_tables.c
    src/cpu/z80_disasm.c
    src/cpu/ctc.c
    src/cpu/sio.c
    src/cpu/pio.c
    src/memory/mmu.c
    src/memory/dcp.c
    src/video/video.c
    src/video/palette.c
    src/video/accel.c
    src/audio/audio.c
    src/audio/ay8910.c
    src/audio/covox.c
    src/audio/beeper.c
    src/disk/ide.c
    src/disk/fdd.c
    src/disk/atapi.c
    src/disk/image.c
    src/input/keyboard.c
    src/input/mouse.c
    src/input/joystick.c
    src/periph/rtc.c
    src/periph/isa.c
    src/debug/debugger.c
    src/debug/breakpoint.c
    src/debug/dbg_ui.c
    src/debug/trace.c
    src/platform/platform.c
    src/platform/sdl_backend.c
)

add_executable(spemulator ${SOURCES})

target_include_directories(spemulator PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(spemulator PRIVATE SDL3::SDL3)

if(APPLE)
    # macOS frameworks
    target_link_libraries(spemulator PRIVATE "-framework Cocoa")
elseif(UNIX)
    target_link_libraries(spemulator PRIVATE m)
elseif(WIN32)
    target_link_libraries(spemulator PRIVATE ws2_32 winmm)
endif()

# Install
install(TARGETS spemulator RUNTIME DESTINATION bin)
install(FILES config/default.ini DESTINATION share/spemulator)
