cmake_minimum_required(VERSION 3.22)
project(RandomChopSampler VERSION 1.0.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)

# Pinned for repeatable builds. CMake downloads JUCE during configuration, so
# contributors and CI do not need a separate JUCE installation.
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.13
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(RandomChopSampler
    COMPANY_NAME "Random Chop Audio"
    BUNDLE_ID "com.randomchopaudio.randomchopsampler"
    PLUGIN_MANUFACTURER_CODE RChp
    PLUGIN_CODE RCS1
    FORMATS VST3 Standalone
    PRODUCT_NAME "Random Chop Sampler"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

# The source uses the Projucer-style aggregate include. Generate it explicitly
# for CMake builds so clean CI runners can resolve <JuceHeader.h>.
juce_generate_juce_header(RandomChopSampler)

target_sources(RandomChopSampler PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/SampleManager.cpp
    Source/SampleManager.h
    Source/RandomSamplerVoice.cpp
    Source/RandomSamplerVoice.h
    Source/RandomizationEngine.h
    Source/SourceSelection.h)

target_compile_definitions(RandomChopSampler PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(RandomChopSampler PRIVATE
    juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)

if (BUILD_TESTING)
    juce_add_console_app(RandomChopSamplerTests
        PRODUCT_NAME "Random Chop Sampler Tests")
    juce_generate_juce_header(RandomChopSamplerTests)
    target_sources(RandomChopSamplerTests PRIVATE
        Tests/Phase1Tests.cpp
        Source/SampleManager.cpp
        Source/SampleManager.h
        Source/RandomizationEngine.h
        Source/SourceSelection.h)
    target_include_directories(RandomChopSamplerTests PRIVATE Source)
    target_compile_definitions(RandomChopSamplerTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(RandomChopSamplerTests PRIVATE
        juce::juce_audio_formats
        PUBLIC juce::juce_recommended_config_flags
               juce::juce_recommended_warning_flags)
    add_test(NAME RandomChopSamplerTests COMMAND RandomChopSamplerTests)
endif()
