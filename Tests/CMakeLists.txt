cmake_minimum_required(VERSION 3.15)

# Fetch Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(googletest)

# Test executable
add_executable(SynthesiaAITests
    GeneratorTests.cpp
    JSONParsingTests.cpp
    ScaleTests.cpp
)

target_compile_definitions(SynthesiaAITests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=0
)

# Tests compile juce_core with JUCE_USE_CURL=0. Only link libcurl where
# find_package succeeded (not Windows GHA).
set(SYNTHESIA_TEST_CURL_LIBS "")
if(NOT WIN32)
    find_package(CURL REQUIRED)
    set(SYNTHESIA_TEST_CURL_LIBS CURL::libcurl)
endif()

target_link_libraries(SynthesiaAITests
    PRIVATE
        gtest_main
        juce::juce_audio_basics
        juce::juce_core
        ${SYNTHESIA_TEST_CURL_LIBS}
)

# Include source directories for testing
target_include_directories(SynthesiaAITests PRIVATE
    ${CMAKE_SOURCE_DIR}/Source
    ${CMAKE_SOURCE_DIR}/Source/Generators
)

# Add sources to test (without main entry point)
target_sources(SynthesiaAITests PRIVATE
    ${CMAKE_SOURCE_DIR}/Source/Generators/MidiGenerator.cpp
    ${CMAKE_SOURCE_DIR}/Source/Generators/MarkovGenerator.cpp
    ${CMAKE_SOURCE_DIR}/Source/Generators/LLMGenerator.cpp
    ${CMAKE_SOURCE_DIR}/Source/Generators/ArrangementPlan.cpp
    ${CMAKE_SOURCE_DIR}/Source/Generators/GenreProfiles.cpp
    ${CMAKE_SOURCE_DIR}/Source/Music/MusicTheory.cpp
    ${CMAKE_SOURCE_DIR}/Source/StyleProfile.cpp
    ${CMAKE_SOURCE_DIR}/Source/MidiImporter.cpp
    ${CMAKE_SOURCE_DIR}/Source/MidiExporter.cpp
)

include(GoogleTest)
gtest_discover_tests(SynthesiaAITests)
