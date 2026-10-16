cmake_minimum_required(VERSION 3.22)

project(SeaboardConfig VERSION 0.1.0 LANGUAGES C CXX)

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
# Phase 1 target: seaprobe
#   Minimal console app. Its sole Phase-1 job is to force the whole
#   roli_blocks_basics module to compile and link against JUCE 8.
#   It opens no MIDI port and performs no device I/O in Phase 1.
# ---------------------------------------------------------------------------
juce_add_console_app(seaprobe
    PRODUCT_NAME "seaprobe")

target_sources(seaprobe PRIVATE
    src/seaprobe/Main.cpp)

target_compile_definitions(seaprobe PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    DONT_SET_USING_JUCE_NAMESPACE=1)

target_link_libraries(seaprobe PRIVATE
    roli_blocks_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
