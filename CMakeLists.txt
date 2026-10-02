# AeroSeed FX - configuration de compilation (JUCE 7 est telecharge automatiquement)
cmake_minimum_required (VERSION 3.22)
project (AeroSeedFX VERSION 1.0.0)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

include (FetchContent)
FetchContent_Declare (JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable (JUCE)

juce_add_plugin (AeroSeedFX
    COMPANY_NAME             "AeroSeed"
    PRODUCT_NAME             "AeroSeed FX"
    PLUGIN_MANUFACTURER_CODE Aerd
    PLUGIN_CODE              Asfx
    FORMATS                  VST3 Standalone
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    VST3_CATEGORIES          Fx
    COPY_PLUGIN_AFTER_BUILD  FALSE)

juce_generate_juce_header (AeroSeedFX)   # le code utilise <JuceHeader.h>

target_sources (AeroSeedFX PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions (AeroSeedFX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

if (MSVC)
    target_compile_options (AeroSeedFX PRIVATE /utf-8 /bigobj)   # gros fichiers d'images integrees
endif()

target_link_libraries (AeroSeedFX
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
