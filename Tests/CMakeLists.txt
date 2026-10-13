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

# Find CURL for LLM tests
find_package(CURL REQUIRED)

target_link_libraries(SynthesiaAITests
    PRIVATE
        gtest_main
        juce::juce_audio_basics
        juce::juce_core
        CURL::libcurl
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
)

include(GoogleTest)
gtest_discover_tests(SynthesiaAITests)
