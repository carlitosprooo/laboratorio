# BANKMASTER — BY KALOX · art generator (app icon, installer artwork, brand sheet).
# Separate project: it does not touch the plug-in build.
#   cmake -S Design/Art -B build-art -DJUCE_DIR=/path/to/JUCE && cmake --build build-art
#   build-art/bankmaster_art_artefacts/Release/bankmaster_art Resources/Fonts .
cmake_minimum_required (VERSION 3.22)
project (BANKMASTER_ART VERSION 2.0.0 LANGUAGES C CXX)
set (CMAKE_CXX_STANDARD 17)
set (JUCE_DIR "" CACHE PATH "JUCE checkout")
if (JUCE_DIR)
    add_subdirectory (${JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else ()
    include (FetchContent)
    FetchContent_Declare (JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable (JUCE)
endif ()
juce_add_console_app (bankmaster_art PRODUCT_NAME "bankmaster_art")
target_sources (bankmaster_art PRIVATE Main.cpp ../../Source/Plugin/UI/DesignSystem.h)
target_include_directories (bankmaster_art PRIVATE ../../Source/Plugin/UI)
target_compile_definitions (bankmaster_art PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries (bankmaster_art PRIVATE juce::juce_graphics juce::juce_recommended_config_flags)
