cmake_minimum_required(VERSION 3.22)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" MAKOSIZERX_DEFAULT_VERSION LIMIT_COUNT 1)
set(MAKOSIZERX_VERSION "${MAKOSIZERX_DEFAULT_VERSION}" CACHE STRING "MakosizerX semantic version")
if(NOT MAKOSIZERX_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "MAKOSIZERX_VERSION must use MAJOR.MINOR.PATCH format")
endif()

project(MakosizerX VERSION ${MAKOSIZERX_VERSION} LANGUAGES C CXX)

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
        makosizerx_background.png
        dark_metal_knob.png
)

target_sources(MakosizerX
    PRIVATE
        MakosizerXDSP.cpp
        LCDDisplay.cpp
        LedMeter.cpp
        MakosizerXLookAndFeel.cpp
        PresetManager.cpp
        PluginEditor.cpp
        PluginProcessor.cpp
)

target_compile_features(MakosizerX PRIVATE cxx_std_17)

target_compile_definitions(MakosizerX
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(MakosizerX
    PRIVATE
        juce::juce_dsp
        juce::juce_audio_utils
        MakosizerXAssets
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

include(CTest)
if(BUILD_TESTING)
    add_executable(MakosizerXDSPTests
        tests/MakosizerXDSPTests.cpp
        MakosizerXDSP.cpp
    )
    target_compile_features(MakosizerXDSPTests PRIVATE cxx_std_17)
    target_include_directories(MakosizerXDSPTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(MakosizerXDSPTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_link_libraries(MakosizerXDSPTests PRIVATE juce::juce_dsp)
    add_test(NAME MakosizerXDSPTests COMMAND MakosizerXDSPTests)
endif()
