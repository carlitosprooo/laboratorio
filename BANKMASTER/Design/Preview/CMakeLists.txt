# BANKMASTER V2 design previews (mockups). Separate project: does not touch the plugin build.
#   cmake -S Design/Preview -B build-design -DJUCE_DIR=/path/to/JUCE && cmake --build build-design
#   build-design/bankmaster_design_preview_artefacts/Release/bankmaster_design_preview Design/Fonts Design/mockups
cmake_minimum_required (VERSION 3.22)
project (BANKMASTER_DESIGN_PREVIEW VERSION 2.0.0 LANGUAGES C CXX)
set (CMAKE_CXX_STANDARD 17)
set (JUCE_DIR "" CACHE PATH "JUCE checkout")
if (JUCE_DIR)
    add_subdirectory (${JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else ()
    include (FetchContent)
    FetchContent_Declare (JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable (JUCE)
endif ()
juce_add_console_app (bankmaster_design_preview PRODUCT_NAME "bankmaster_design_preview")
target_sources (bankmaster_design_preview PRIVATE Main.cpp Sim.h ../V2/DesignSystem.h)
target_include_directories (bankmaster_design_preview PRIVATE ../../Source/DSP)
target_compile_definitions (bankmaster_design_preview PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries (bankmaster_design_preview PRIVATE juce::juce_graphics juce::juce_recommended_config_flags)
