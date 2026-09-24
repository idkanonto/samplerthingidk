cmake_minimum_required(VERSION 3.22)
project(RandomChopSampler VERSION 3.0.0 LANGUAGES CXX)

include(CTest)

option(RECOMPILER_USE_NATIVE_EDITOR
    "Use the retained native JUCE editor instead of the WebView editor" OFF)
option(RECOMPILER_REBUILD_FRONTEND
    "Rebuild the React frontend when Node/npm are available" ON)

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

FetchContent_Declare(
    signalsmith_stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG 57b93f4e9206a089a45387eaa39bdc9f310d3308
    GIT_SHALLOW FALSE
    GIT_PROGRESS TRUE)
FetchContent_MakeAvailable(signalsmith_stretch)
FetchContent_GetProperties(JUCE SOURCE_DIR juce_source_dir)
FetchContent_GetProperties(signalsmith-linear SOURCE_DIR signalsmith_linear_source_dir)

juce_add_plugin(RandomChopSampler
    COMPANY_NAME "recompiler.dll"
    BUNDLE_ID "com.randomchopaudio.randomchopsampler"
    PLUGIN_MANUFACTURER_CODE RChp
    PLUGIN_CODE RCS1
    FORMATS VST3 Standalone
    PRODUCT_NAME "recompiler.dll"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    NEEDS_WEBVIEW2 TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)

# The source uses the Projucer-style aggregate include. Generate it explicitly
# for CMake builds so clean CI runners can resolve <JuceHeader.h>.
juce_generate_juce_header(RandomChopSampler)

target_sources(RandomChopSampler PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/WebViewEditor.cpp
    Source/WebViewEditor.h
    Source/SampleManager.cpp
    Source/SampleManager.h
    Source/PreparedSampleData.h
    Source/RandomSamplerVoice.cpp
    Source/RandomSamplerVoice.h
    Source/VoicePool.h
    Source/HostGrid.h
    Source/StateMigration.h
    Source/TemporalEffects.cpp
    Source/TemporalEffects.h
    Source/CreativeEffects.cpp
    Source/CreativeEffects.h
    Source/SpectralDraw.cpp
    Source/SpectralDraw.h
    Source/RandomizationEngine.h
    Source/HarmonicPitch.h
    Source/PlaybackRegion.h
    Source/SourceSelection.h)

set(frontend_directory "${CMAKE_CURRENT_SOURCE_DIR}/frontend")
set(frontend_dist_files
    "${frontend_directory}/dist/index.html"
    "${frontend_directory}/dist/assets/app.js"
    "${frontend_directory}/dist/assets/app.css"
    "${frontend_directory}/dist/assets/anton.ttf")

if (RECOMPILER_REBUILD_FRONTEND)
    find_program(NPM_EXECUTABLE npm)
    if (NPM_EXECUTABLE)
        add_custom_command(
            OUTPUT "${frontend_directory}/dist/.frontend-built"
            COMMAND "${NPM_EXECUTABLE}" ci --no-audit --no-fund
            COMMAND "${NPM_EXECUTABLE}" run build
            COMMAND ${CMAKE_COMMAND} -E touch "${frontend_directory}/dist/.frontend-built"
            WORKING_DIRECTORY "${frontend_directory}"
            DEPENDS
                "${frontend_directory}/package.json"
                "${frontend_directory}/package-lock.json"
                "${frontend_directory}/index.html"
                "${frontend_directory}/vite.config.ts"
                "${frontend_directory}/tsconfig.app.json"
                "${frontend_directory}/src/App.tsx"
                "${frontend_directory}/src/VisualCanvases.tsx"
                "${frontend_directory}/src/juceBridge.ts"
                "${frontend_directory}/src/main.tsx"
                "${frontend_directory}/src/styles.css"
                "${frontend_directory}/src/assets/anton.ttf"
            BYPRODUCTS ${frontend_dist_files}
            COMMENT "Building embedded React WebView frontend"
            VERBATIM)
        add_custom_target(RecompilerFrontendBuild
            DEPENDS "${frontend_directory}/dist/.frontend-built")
    else()
        message(WARNING
            "npm was not found; packaging the checked-in frontend/dist assets")
    endif()
endif()

foreach(frontend_asset IN LISTS frontend_dist_files)
    if (NOT EXISTS "${frontend_asset}")
        message(FATAL_ERROR
            "Missing ${frontend_asset}. Run npm install and npm run build in frontend/.")
    endif()
endforeach()

juce_add_binary_data(RecompilerWebAssets
    NAMESPACE BinaryData
    SOURCES ${frontend_dist_files})
if (TARGET RecompilerFrontendBuild)
    add_dependencies(RecompilerWebAssets RecompilerFrontendBuild)
endif()

target_compile_definitions(RandomChopSampler PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    RECOMPILER_USE_NATIVE_EDITOR=$<BOOL:${RECOMPILER_USE_NATIVE_EDITOR}>)

target_link_libraries(RandomChopSampler PRIVATE
    juce::juce_audio_utils
    juce::juce_gui_extra
    RecompilerWebAssets
    signalsmith-stretch
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)

add_custom_command(TARGET RandomChopSampler_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:RandomChopSampler_VST3>/../Resources"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        "$<TARGET_FILE_DIR:RandomChopSampler_VST3>/../Resources/THIRD_PARTY_NOTICES.md"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${juce_source_dir}/LICENSE.md"
        "$<TARGET_FILE_DIR:RandomChopSampler_VST3>/../Resources/JUCE_LICENSE.md"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${signalsmith_linear_source_dir}/LICENSE.txt"
        "$<TARGET_FILE_DIR:RandomChopSampler_VST3>/../Resources/SIGNALSMITH_LINEAR_LICENSE.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/Licenses/ANTON_OFL.txt"
        "$<TARGET_FILE_DIR:RandomChopSampler_VST3>/../Resources/ANTON_OFL.txt"
    COMMENT "Copying third-party notices into the VST3 bundle")

if (BUILD_TESTING)
    juce_add_console_app(RandomChopSamplerTests
        PRODUCT_NAME "recompiler.dll Tests")
    juce_generate_juce_header(RandomChopSamplerTests)
    target_sources(RandomChopSamplerTests PRIVATE
        Tests/Phase1Tests.cpp
        Source/SampleManager.cpp
        Source/SampleManager.h
        Source/PreparedSampleData.h
        Source/RandomSamplerVoice.cpp
        Source/RandomSamplerVoice.h
        Source/VoicePool.h
        Source/HostGrid.h
        Source/StateMigration.h
        Source/TemporalEffects.cpp
        Source/TemporalEffects.h
        Source/CreativeEffects.cpp
        Source/CreativeEffects.h
        Source/SpectralDraw.cpp
        Source/SpectralDraw.h
        Source/RandomizationEngine.h
        Source/HarmonicPitch.h
        Source/PlaybackRegion.h
        Source/SourceSelection.h)
    target_include_directories(RandomChopSamplerTests PRIVATE Source)
    target_compile_definitions(RandomChopSamplerTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_USE_MP3AUDIOFORMAT=1)
    target_link_libraries(RandomChopSamplerTests PRIVATE
        juce::juce_audio_formats
        juce::juce_data_structures
        signalsmith-stretch
        PUBLIC juce::juce_recommended_config_flags
               juce::juce_recommended_warning_flags)
    add_test(NAME RandomChopSamplerTests COMMAND RandomChopSamplerTests)
endif()

