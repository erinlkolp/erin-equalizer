cmake_minimum_required(VERSION 3.22)
project(ErinEqualizer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Catch2)

juce_add_plugin(ErinEqualizer
    COMPANY_NAME "Erin Audio"
    PLUGIN_MANUFACTURER_CODE Erin
    PLUGIN_CODE EqGr
    FORMATS VST3 Standalone
    PRODUCT_NAME "Erin Equalizer"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
)

target_sources(ErinEqualizer PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/EqProcessor.cpp
    src/BandComponent.cpp
    src/LevelMeter.cpp
    src/Toolbar.cpp
    src/SpectrumAnalyzer.cpp
)

target_include_directories(ErinEqualizer PRIVATE src)

target_compile_definitions(ErinEqualizer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ErinEqualizer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Test target
add_executable(ErinEqualizerTests
    tests/BiquadFilterTest.cpp
    tests/EqProcessorTest.cpp
    src/EqProcessor.cpp
)

target_include_directories(ErinEqualizerTests PRIVATE src)

target_compile_definitions(ErinEqualizerTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STANDALONE_APPLICATION=0
)

target_link_libraries(ErinEqualizerTests
    PRIVATE
        Catch2::Catch2WithMain
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_dsp
)

include(CTest)
include(Catch)
catch_discover_tests(ErinEqualizerTests)
