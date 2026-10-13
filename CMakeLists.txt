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

# VST3 on Windows, macOS and Linux. AU is added on Apple hosts.
# Build the VST3 target explicitly, e.g. cmake --build build --target SynthesiaAI_VST3
if(APPLE)
    set(SYNTHESIA_FORMATS VST3 AU Standalone)
else()
    set(SYNTHESIA_FORMATS VST3 Standalone)
endif()

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
    FORMATS ${SYNTHESIA_FORMATS}
    PRODUCT_NAME "Synthesia AI"
)

# Source files (shared with the SynthesiaRender host below)
set(SYNTHESIA_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Generators/MidiGenerator.cpp
    Source/Generators/MarkovGenerator.cpp
    Source/Generators/LLMGenerator.cpp
    Source/Generators/GenreProfiles.cpp
    Source/Generators/ArrangementGenerator.cpp
    Source/Generators/ArrangementPlan.cpp
    Source/Music/MusicTheory.cpp
    Source/SimpleSynth.cpp
    Source/SynthPatch.cpp
    Source/DrumLibrary.cpp
    Source/MidiExporter.cpp
    Source/MidiImporter.cpp
    Source/StyleProfile.cpp
    Source/PresetStore.cpp
    Source/LicenseStub.cpp
)
target_sources(SynthesiaAI PRIVATE ${SYNTHESIA_SOURCES})

file(GLOB SYNTHESIA_DRUM_WAVS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Samples/*.wav")
juce_add_binary_data(SynthesiaDrumSamples
    HEADER_NAME DrumBinaryData.h
    NAMESPACE DrumBinaryData
    SOURCES ${SYNTHESIA_DRUM_WAVS})
set_target_properties(SynthesiaDrumSamples PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

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
        SynthesiaDrumSamples
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Offline render host (headless "DAW" used for demos/smoke checks)
option(SYNTHESIA_BUILD_RENDER_HOST "Build the SynthesiaRender offline host" ON)
if(SYNTHESIA_BUILD_RENDER_HOST)
    juce_add_console_app(SynthesiaRender PRODUCT_NAME "SynthesiaRender")
    target_sources(SynthesiaRender PRIVATE
        Tools/RenderHost/RenderHost.cpp
        ${SYNTHESIA_SOURCES})
    target_include_directories(SynthesiaRender PRIVATE Source)
    target_compile_definitions(SynthesiaRender PRIVATE
        JucePlugin_Name="Synthesia AI"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=1)
    target_link_libraries(SynthesiaRender PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        CURL::libcurl
        SynthesiaDrumSamples
        juce::juce_recommended_config_flags)
endif()

# Tests
enable_testing()
add_subdirectory(Tests)
