cmake_minimum_required(VERSION 3.22)

# Static MSVC runtime -> the plugin needs no VC++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

project(DKD VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(FIS_BUILD_CLAP  "Build CLAP format (clap-juce-extensions)" ON)
option(FIS_BUILD_TOOLS "Build offline render / screenshot tool"   ON)

include(FetchContent)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    add_subdirectory(external/JUCE)
else()
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(juce)
endif()

set(FIS_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND FIS_FORMATS AU)
endif()

# DKD (Dual Kick Deployment): rutherford's engine in a two-screen music-device editor. NEW plugin
# identity (own name / bundle ID / code), so FL sees it next to rutherford, not instead of it.
# FISSION by sonicsystems. The product name (= the .vst3 file name), the bundle ID and the codes are the identity FL
# Studio stores in projects: never change them once released.
juce_add_plugin(DKD
    COMPANY_NAME              "sonicsystems"
    PRODUCT_NAME              "dkd"
    PLUGIN_NAME               "dkd"
    BUNDLE_ID                 "com.sonicsystems.dkd"
    PLUGIN_MANUFACTURER_CODE  Sncs
    PLUGIN_CODE               Sdkd
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           Instrument Synth
    AU_MAIN_TYPE              kAudioUnitType_MusicDevice
    FORMATS                   ${FIS_FORMATS})

set(FIS_SOURCES
    Source/Params.cpp
    Source/PluginProcessor.cpp
    Source/Digi.cpp
    Source/Views.cpp
    Source/Hw.cpp
    Source/PluginEditor.cpp)

target_sources(DKD PRIVATE ${FIS_SOURCES})

set(FIS_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0)

target_compile_definitions(DKD PUBLIC ${FIS_DEFS})

target_link_libraries(DKD
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(FIS_BUILD_CLAP)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/clap-juce-extensions/CMakeLists.txt")
        add_subdirectory(external/clap-juce-extensions EXCLUDE_FROM_ALL)
    else()
        FetchContent_Declare(clap-juce-extensions
            GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
            GIT_TAG        55525c9858d4b25687be7759a5e0f70eccef218e
            GIT_SUBMODULES_RECURSE TRUE)
        FetchContent_MakeAvailable(clap-juce-extensions)
    endif()

    clap_juce_extensions_plugin(TARGET DKD
        CLAP_ID "com.sonicsystems.dkd"
        CLAP_FEATURES instrument synthesizer drum stereo)
endif()

# ---- offline render + GUI screenshot (the test)
if(FIS_BUILD_TOOLS)
    juce_add_console_app(DKDRender PRODUCT_NAME "DKDRender")
    target_sources(DKDRender PRIVATE Tools/RenderDemo.cpp ${FIS_SOURCES})
    target_include_directories(DKDRender PRIVATE Source)
    target_compile_definitions(DKDRender PRIVATE ${FIS_DEFS} FIS_STANDALONE_TOOL=1)
    target_link_libraries(DKDRender
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()

# ---- FISSION shared look (src/FISSION_shared: controls, window, cloud chamber)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../FISSION_shared/fission.cmake)
fission_use(DKD)
if(TARGET DKDRender)
    fission_use(DKDRender)
endif()
