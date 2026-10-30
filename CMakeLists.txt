cmake_minimum_required(VERSION 3.22)

project(GRAB VERSION 0.1.65 LANGUAGES C CXX)  # bump with each release; tag vX.Y.Z to match

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# This machine has a global vcpkg MSBuild integration that injects an
# "applocal" post-build step invoking pwsh.exe (PowerShell Core, not installed
# here), which errors on every target. We don't use vcpkg; disable its
# integration for OUR generated project only. Nothing system-wide is changed.
if(CMAKE_GENERATOR MATCHES "Visual Studio")
    set(CMAKE_VS_GLOBALS "VcpkgEnabled=false")
endif()

# ---------------------------------------------------------------------------
# JUCE 8 (fetched and pinned; never installed system-wide, never committed)
# ---------------------------------------------------------------------------
include(FetchContent)

set(JUCE_TAG "8.0.15" CACHE STRING "JUCE git tag to build against")

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${JUCE_TAG}
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# roli_blocks_basics  (vendored copy, patched for JUCE 8 — see PATCHES.md)
# ---------------------------------------------------------------------------
juce_add_module("${CMAKE_CURRENT_SOURCE_DIR}/third_party/roli_blocks_basics")

# ---------------------------------------------------------------------------
# Portable SysEx decoder helpers (no JUCE; also parses offline hex dumps)
# ---------------------------------------------------------------------------
add_subdirectory(libs/sea_sysex)

# ---------------------------------------------------------------------------
# seadecode: portable offline decoder CLI (no JUCE). Parses hex dumps and
# USB-MIDI captures (Wireshark/USBPcap) into decoded ROLI messages.
# ---------------------------------------------------------------------------
add_executable(seadecode src/seadecode/Main.cpp)
target_link_libraries(seadecode PRIVATE sea_sysex)
target_compile_features(seadecode PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
# Phase 1 target: seaprobe
#   Minimal console app. Its sole Phase-1 job is to force the whole
#   roli_blocks_basics module to compile and link against JUCE 8.
#   It opens no MIDI port and performs no device I/O in Phase 1.
# ---------------------------------------------------------------------------
juce_add_console_app(seaprobe
    PRODUCT_NAME "seaprobe")

target_sources(seaprobe PRIVATE
    src/seaprobe/Main.cpp)

target_include_directories(seaprobe PRIVATE src/common)

target_compile_definitions(seaprobe PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    DONT_SET_USING_JUCE_NAMESPACE=1
    # Make roli_blocks_basics log its handshake / keepalive / topology events
    # (e.g. "API Try <uid>" when beginAPIMode is sent) to stderr.
    LOG_BLOCKS_CONNECTIVITY=1)

target_link_libraries(seaprobe PRIVATE
    sea_sysex
    roli_blocks_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Phase 4 target: GRAB (minimal JUCE GUI)
#   One control per device-reported config item; Save to device; JSON presets.
# ---------------------------------------------------------------------------
juce_add_gui_app(GRAB
    PRODUCT_NAME "GRAB")

target_sources(GRAB PRIVATE
    src/gui/Main.cpp
    src/gui/MainComponent.cpp
    src/gui/Rise2Component.cpp)

target_include_directories(GRAB PRIVATE src/common src/gui)

target_compile_definitions(GRAB PRIVATE
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    DONT_SET_USING_JUCE_NAMESPACE=1
    LOG_BLOCKS_CONNECTIVITY=1
    GRAB_VERSION="${PROJECT_VERSION}")

# Treat source as UTF-8 and emit UTF-8 narrow literals (MSVC otherwise uses the system
# codepage, mangling any non-ASCII text). Pair with grab::u8()/String::fromUTF8 to decode.
if(MSVC)
    target_compile_options(GRAB PRIVATE /utf-8)
endif()

target_link_libraries(GRAB PRIVATE
    sea_sysex
    roli_blocks_basics
    juce::juce_gui_extra
    juce::juce_audio_devices
    juce::juce_cryptography
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
