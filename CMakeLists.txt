cmake_minimum_required(VERSION 3.22)

project(SimpleDelay VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/JUCE/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at libs/JUCE. See README.md for the one-time clone command.")
endif()

add_subdirectory(libs/JUCE)

juce_add_plugin(SimpleDelay
    COMPANY_NAME "jirwo"
    PLUGIN_MANUFACTURER_CODE Jirw
    PLUGIN_CODE Sdly
    PRODUCT_NAME "Simple Delay"
    FORMATS Standalone VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(SimpleDelay PRIVATE
    Source/PluginProcessor.cpp)

target_include_directories(SimpleDelay PRIVATE Source)

target_compile_definitions(SimpleDelay PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SimpleDelay PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
