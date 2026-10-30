cmake_minimum_required(VERSION 3.22)
project(PhantomAsym VERSION 1.0.17)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(PLUGIN_FORMATS VST3 AU)
else()
    set(PLUGIN_FORMATS VST3)
endif()

juce_add_plugin(PhantomAsym
    COMPANY_NAME "DIY"
    PLUGIN_MANUFACTURER_CODE Mest
    PLUGIN_CODE Pasm
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "rmd-1"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(PhantomAsymAssets SOURCES
    Resources/ui_bg.png
    Resources/ui_depth_body.png
    Resources/ui_depth_ptr.png
    Resources/ui_fade_body.png
    Resources/ui_fade_ptr.png
    Resources/ui_mode_half.png
    Resources/ui_mode_whole.png
    Resources/ui_fader_slot.png
    Resources/ui_fader_cap.png
    Resources/ui_btn_off.png
    Resources/ui_btn_on.png
    Resources/InterRegular.ttf
    Resources/InterLight.ttf)

target_sources(PhantomAsym PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h)

target_compile_definitions(PhantomAsym PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(PhantomAsym
    PRIVATE PhantomAsymAssets
            juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)
