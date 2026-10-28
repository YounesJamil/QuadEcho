cmake_minimum_required(VERSION 3.22)

# Mac: lauffaehig ab macOS 11; Universal Binary (Apple Silicon + Intel) per -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Aelteste unterstuetzte macOS-Version")

project(QuadEcho VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
# Lokal unter Windows liegt JUCE auf Q: (schneller als Y:), sonst (Mac, CI) im Build-Ordner
if(WIN32 AND EXISTS "Q:/")
    set(QUADECHO_DEPS_DEFAULT "Q:/QuadEcho/deps")
else()
    set(QUADECHO_DEPS_DEFAULT "${CMAKE_BINARY_DIR}/_deps")
endif()
set(QUADECHO_DEPS_DIR "${QUADECHO_DEPS_DEFAULT}" CACHE PATH "Ablage fuer JUCE")
set(FETCHCONTENT_BASE_DIR ${QUADECHO_DEPS_DIR})
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(QuadEcho
    COMPANY_NAME "Younes Jamil"
    COMPANY_COPYRIGHT "Copyright (c) 2026 Younes Jamil"
    PLUGIN_DESCRIPTION "QUAD ECHO by Younes Jamil: four delay units, routing algorithms and 3D placement"
    PLUGIN_MANUFACTURER_CODE Qdec
    PLUGIN_CODE Qecm
    BUNDLE_ID "com.younesjamil.quadecho"
    FORMATS VST3 AU Standalone          # AU nur auf dem Mac (Logic, GarageBand); JUCE laesst es unter Windows weg
    PRODUCT_NAME "QUAD ECHO"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

# Eingebaute Schrift: Barlow Semi Condensed (SIL Open Font License, siehe Resources/Fonts/OFL.txt),
# damit die Oberflaeche auf Windows und Mac gleich aussieht
juce_add_binary_data(QuadEchoFonts SOURCES
    Resources/Fonts/BarlowSemiCondensed-Regular.ttf
    Resources/Fonts/BarlowSemiCondensed-SemiBold.ttf
    Resources/Fonts/BarlowSemiCondensed-Bold.ttf)

set(QUADECHO_DSP_SOURCES
    Source/DelayUnit.cpp
    Source/Spatializer.cpp
    Source/QuadEngine.cpp
    Source/GlobalFilter.cpp
    Source/NoiseSource.cpp)

target_sources(QuadEcho PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Params.cpp
    Source/Presets.cpp
    ${QUADECHO_DSP_SOURCES})

target_compile_definitions(QuadEcho PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(QuadEcho PRIVATE
    QuadEchoFonts
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# DSP-Tests und Offline-Renders (ohne Plugin-Host)
juce_add_console_app(QuadEchoTests PRODUCT_NAME "QuadEchoTests")
target_sources(QuadEchoTests PRIVATE tests/Tests.cpp tests/Harness.cpp tests/Audit.cpp ${QUADECHO_DSP_SOURCES})
target_link_libraries(QuadEchoTests PRIVATE
    juce::juce_audio_formats
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# Rendert den Editor ohne Fenster in ein PNG (UI-Pruefung)
juce_add_console_app(QuadEchoUiShot PRODUCT_NAME "QuadEchoUiShot")
target_sources(QuadEchoUiShot PRIVATE
    tests/UiShot.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Params.cpp
    Source/Presets.cpp
    ${QUADECHO_DSP_SOURCES})
target_compile_definitions(QuadEchoUiShot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(QuadEchoUiShot PRIVATE
    QuadEchoFonts
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags)
