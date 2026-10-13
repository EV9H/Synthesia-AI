cmake_minimum_required(VERSION 3.15)

project(SynthesiaAI VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch JUCE
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# Plugin configuration
juce_add_plugin(SynthesiaAI
    COMPANY_NAME "SynthesiaAI"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Synt
    PLUGIN_CODE SyAI
    FORMATS VST3 Standalone
    PRODUCT_NAME "Synthesia AI"
)

# macOS-specific: Add AU format
if(APPLE)
    set_target_properties(SynthesiaAI PROPERTIES
        JUCE_FORMATS "VST3;AU;Standalone"
    )
endif()

# Source files
target_sources(SynthesiaAI
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Generators/MidiGenerator.cpp
        Source/Generators/MarkovGenerator.cpp
        Source/Generators/LLMGenerator.cpp
        Source/SimpleSynth.cpp
        Source/MidiExporter.cpp
)

# JUCE modules
target_compile_definitions(SynthesiaAI
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=1
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# Find CURL
find_package(CURL REQUIRED)

target_link_libraries(SynthesiaAI
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        CURL::libcurl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Tests
enable_testing()
add_subdirectory(Tests)
