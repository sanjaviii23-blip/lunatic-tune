cmake_minimum_required(VERSION 3.22)

# macOS: binario universal (Apple Silicon + Intel). Debe definirse antes de project().
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "")

project(LunaticTune VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Ruta local opcional a JUCE:  cmake -B build -DLUNATIC_JUCE_DIR=/ruta/a/JUCE
set(LUNATIC_JUCE_DIR "" CACHE PATH "Carpeta de JUCE ya descargada (opcional)")
if(LUNATIC_JUCE_DIR)
    add_subdirectory(${LUNATIC_JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.6
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(LUNATIC_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND LUNATIC_FORMATS AU)
endif()

juce_add_plugin(LunaticTune
    COMPANY_NAME "Lunatic"
    PLUGIN_MANUFACTURER_CODE Lntc
    PLUGIN_CODE Lntn
    PRODUCT_NAME "LUNATIC TUNE"
    BUNDLE_ID com.lunatic.tune
    FORMATS ${LUNATIC_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx "Pitch Shift"
    AU_MAIN_TYPE kAudioUnitType_MusicEffect)

target_sources(LunaticTune PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/LunaticLookAndFeel.h
    Source/DSP/AutoTuneEngine.h
    Source/DSP/PitchDetector.h
    Source/DSP/PsolaShifter.h
    Source/DSP/Scales.h)

target_compile_definitions(LunaticTune PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(LunaticTune
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

if(MSVC)
    target_compile_options(LunaticTune PRIVATE /O2 /fp:fast /utf-8)
else()
    target_compile_options(LunaticTune PRIVATE -O3)
endif()

# Pruebas del motor DSP (sin JUCE):  cmake -B build -DLUNATIC_BUILD_TESTS=ON
option(LUNATIC_BUILD_TESTS "Compilar las pruebas del DSP" OFF)
if(LUNATIC_BUILD_TESTS)
    add_executable(lunatic_dsp_test Tests/dsp_test.cpp)
    target_include_directories(lunatic_dsp_test PRIVATE Source/DSP)
endif()
