# Portable, dependency-free SysEx framing/decoder helpers.
# No JUCE here on purpose: the same code parses offline Wireshark hex dumps.
add_library(sea_sysex STATIC
    sea_sysex.cpp
    sea_sysex.h)

target_include_directories(sea_sysex PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(sea_sysex PUBLIC cxx_std_17)
