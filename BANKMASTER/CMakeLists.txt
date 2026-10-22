cmake_minimum_required (VERSION 3.22)

# macOS: universal binary (Apple Silicon + Intel) unless the caller overrides it
if (APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set (CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
endif ()
set (CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")

project (BANKMASTER VERSION 2.0.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set_property (GLOBAL PROPERTY USE_FOLDERS ON)

option (BANKMASTER_BUILD_PLUGIN "Build the JUCE plugin (VST3/AU/Standalone)" ON)
option (BANKMASTER_BUILD_TESTS "Build the test executables" ON)
set (BANKMASTER_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (otherwise JUCE 8.0.4 is fetched)")

# ---------------------------------------------------------------------------------------
# DSP core (framework-free, header-only)
# ---------------------------------------------------------------------------------------
add_library (bankmaster_dsp INTERFACE)
target_include_directories (bankmaster_dsp INTERFACE Source/DSP)

if (BANKMASTER_BUILD_TESTS)
    enable_testing ()
    add_executable (bankmaster_dsp_tests Tests/DspTests.cpp Tests/TestUtils.h)
    target_link_libraries (bankmaster_dsp_tests PRIVATE bankmaster_dsp)
    if (NOT MSVC)
        target_compile_options (bankmaster_dsp_tests PRIVATE -Wall -Wextra)
    endif ()
    add_test (NAME dsp_tests COMMAND bankmaster_dsp_tests)
endif ()

if (NOT BANKMASTER_BUILD_PLUGIN)
    return ()
endif ()

# ---------------------------------------------------------------------------------------
# JUCE
# ---------------------------------------------------------------------------------------
if (BANKMASTER_JUCE_DIR)
    add_subdirectory (${BANKMASTER_JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else ()
    include (FetchContent)
    FetchContent_Declare (JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable (JUCE)
endif ()

set (BANKMASTER_FORMATS VST3 Standalone)
if (APPLE)
    list (APPEND BANKMASTER_FORMATS AU)
endif ()

# Identity. COMPANY_NAME is only the vendor text shown by hosts ("Kalox Audio").
# Hosts and saved projects identify the plug-in by the manufacturer / plug-in codes
# (VST3 class IDs and the AU component are derived from Klox + Bkms) and by the
# bundle ID: these must never change.
juce_add_plugin (BankMaster
    PRODUCT_NAME "BANKMASTER"
    COMPANY_NAME "Kalox Audio"
    COMPANY_COPYRIGHT "(c) 2026 Kalox Audio"
    COMPANY_WEBSITE "https://kalox.audio"
    BUNDLE_ID "com.kalox.bankmaster"
    PLUGIN_MANUFACTURER_CODE Klox
    PLUGIN_CODE Bkms
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Icon/icon_1024.png"
    DESCRIPTION "BANKMASTER BY KALOX - 4-in-1 Mastering Suite"
    FORMATS ${BANKMASTER_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE          # never installs anything implicitly
    VST3_CATEGORIES "Fx" "Mastering" "Dynamics" "EQ"
    AU_MAIN_TYPE kAudioUnitType_Effect
    HARDENED_RUNTIME_ENABLED TRUE)

set (BANKMASTER_PLUGIN_SOURCES
    Source/Plugin/Parameters.cpp
    Source/Plugin/PresetManager.cpp
    Source/Plugin/PluginProcessor.cpp
    Source/Plugin/PluginEditor.cpp
    Source/Plugin/UI/Controls.cpp
    Source/Plugin/UI/Displays.cpp
    Source/Plugin/UI/Pages.cpp
    Source/Plugin/UI/Chrome.cpp)

target_sources (BankMaster PRIVATE ${BANKMASTER_PLUGIN_SOURCES})

# UI fonts (Inter + Montserrat, SIL OFL 1.1) embedded in the binary: identical text on macOS and Windows
juce_add_binary_data (BankMasterAssets
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Resources/Fonts/Inter-Regular.otf
        Resources/Fonts/Inter-Medium.otf
        Resources/Fonts/Inter-SemiBold.otf
        Resources/Fonts/Inter-Bold.otf
        Resources/Fonts/Montserrat-ExtraBold.ttf
        Resources/Fonts/Montserrat-SemiBold.ttf)
set_target_properties (BankMasterAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

set (BANKMASTER_JUCE_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_compile_definitions (BankMaster PUBLIC ${BANKMASTER_JUCE_DEFS} JUCE_APPLICATION_NAME_STRING="BANKMASTER")

set (BANKMASTER_JUCE_MODULES
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_gui_extra)

target_link_libraries (BankMaster
    PRIVATE bankmaster_dsp BankMasterAssets ${BANKMASTER_JUCE_MODULES}
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
if (NOT MSVC)
    target_compile_options (BankMaster PRIVATE -Wall -Wextra)
endif ()

# ---------------------------------------------------------------------------------------
# Plugin-level tests (state, presets, automation, A/B, editor snapshot) + offline tools
# ---------------------------------------------------------------------------------------
if (BANKMASTER_BUILD_TESTS)
    juce_add_console_app (bankmaster_plugin_tests PRODUCT_NAME "bankmaster_plugin_tests")
    target_sources (bankmaster_plugin_tests PRIVATE Tests/PluginTests.cpp ${BANKMASTER_PLUGIN_SOURCES})
    target_compile_definitions (bankmaster_plugin_tests PRIVATE ${BANKMASTER_JUCE_DEFS}
        JucePlugin_VersionString="${PROJECT_VERSION}" JucePlugin_Name="BANKMASTER" JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries (bankmaster_plugin_tests PRIVATE bankmaster_dsp BankMasterAssets ${BANKMASTER_JUCE_MODULES}
        juce::juce_recommended_config_flags)
    add_test (NAME plugin_tests COMMAND bankmaster_plugin_tests)
endif ()

juce_add_console_app (bankmaster_render PRODUCT_NAME "bankmaster_render")
target_sources (bankmaster_render PRIVATE Tools/Render.cpp)
target_compile_definitions (bankmaster_render PRIVATE ${BANKMASTER_JUCE_DEFS})
target_link_libraries (bankmaster_render PRIVATE bankmaster_dsp juce::juce_audio_formats juce::juce_audio_basics
    juce::juce_recommended_config_flags)

# Host-side check of a built plug-in: identity, parameters, state and audio (V1 x V2 null test)
juce_add_console_app (bankmaster_hostcheck PRODUCT_NAME "bankmaster_hostcheck")
target_sources (bankmaster_hostcheck PRIVATE Tools/HostCheck.cpp)
target_compile_definitions (bankmaster_hostcheck PRIVATE ${BANKMASTER_JUCE_DEFS} JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_LADSPA=0 JUCE_PLUGINHOST_LV2=0 JUCE_PLUGINHOST_VST=0 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries (bankmaster_hostcheck PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_recommended_config_flags)
