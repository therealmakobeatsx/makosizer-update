cmake_minimum_required(VERSION 3.22)

project(MakosizerX VERSION 1.0.0 LANGUAGES C CXX)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE)

set(MAKOSIZERX_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND MAKOSIZERX_FORMATS AU)
endif()

juce_add_plugin(MakosizerX
    COMPANY_NAME "MakosizerX"
    BUNDLE_ID "com.makosizerx.plugin"
    PLUGIN_MANUFACTURER_CODE Mako
    PLUGIN_CODE MkSz
    FORMATS ${MAKOSIZERX_FORMATS}
    PRODUCT_NAME "MakosizerX"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(MakosizerX)

juce_add_binary_data(MakosizerXAssets
    SOURCES
        Assets/makosizerx_background.png
)

target_sources(MakosizerX
    PRIVATE
        DSP/MakosizerXDSP.cpp
        GUI/LCDDisplay.cpp
        GUI/LedMeter.cpp
        GUI/MakosizerXLookAndFeel.cpp
        PresetManager.cpp
        Source/PluginEditor.cpp
        Source/PluginProcessor.cpp
)

target_compile_features(MakosizerX PRIVATE cxx_std_17)

target_compile_definitions(MakosizerX
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(MakosizerX
    PRIVATE
        juce::juce_audio_utils
        MakosizerXAssets
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
