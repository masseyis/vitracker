cmake_minimum_required(VERSION 3.22)
project(Vitracker VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch JUCE
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
)
FetchContent_MakeAvailable(JUCE)

# Fetch GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

# Fetch RubberBand for time-stretching (using single-file build)
FetchContent_Declare(
    rubberband
    GIT_REPOSITORY https://github.com/breakfastquay/rubberband.git
    GIT_TAG v3.3.0
)
FetchContent_GetProperties(rubberband)
if(NOT rubberband_POPULATED)
    FetchContent_Populate(rubberband)
endif()

# Main application
juce_add_gui_app(Vitracker
    COMPANY_NAME "MasseyIS"
    PRODUCT_NAME "Vitracker"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png")

juce_generate_juce_header(Vitracker)

# Collect Plaits/stmlib DSP source files
file(GLOB_RECURSE DSP_SOURCES
    src/dsp/*.cpp
    src/dsp/*.cc)

# Exclude STM32-specific system files that won't compile on desktop
list(FILTER DSP_SOURCES EXCLUDE REGEX ".*stmlib/system/.*")

target_sources(Vitracker PRIVATE
    src/main.cpp
    src/App.cpp
    src/model/Pattern.cpp
    src/model/Instrument.cpp
    src/model/Chain.cpp
    src/model/Song.cpp
    src/model/Project.cpp
    src/model/ProjectSerializer.cpp
    src/model/PresetManager.cpp
    src/model/DX7PresetBank.cpp
    src/input/ModeManager.cpp
    src/input/KeyHandler.cpp
    src/ui/Screen.cpp
    src/ui/PatternScreen.cpp
    src/ui/ProjectScreen.cpp
    src/ui/InstrumentScreen.cpp
    src/ui/MixerScreen.cpp
    src/ui/ChannelScreen.cpp
    src/ui/SongScreen.cpp
    src/ui/ChainScreen.cpp
    src/ui/WaveformDisplay.cpp
    src/ui/SliceWaveformDisplay.cpp
    src/ui/SamplerScreen.cpp
    src/ui/HelpPopup.cpp
    src/ui/ChordPopup.cpp
    src/ui/AudioSettingsPopup.cpp
    src/audio/Voice.cpp
    src/audio/AudioEngine.cpp
    src/audio/Effects.cpp
    src/audio/BiquadFilter.cpp
    src/audio/TransientShaper.cpp
    src/audio/MultibandOTT.cpp
    src/audio/ChannelStrip.cpp
    src/audio/PlaitsInstrument.cpp
    src/audio/SamplerVoice.cpp
    src/audio/SamplerInstrument.cpp
    src/audio/SlicerVoice.cpp
    src/audio/SlicerInstrument.cpp
    src/audio/VASynthInstrument.cpp
    src/audio/DX7Instrument.cpp
    ${DSP_SOURCES}
    ${rubberband_SOURCE_DIR}/single/RubberBandSingle.cpp)

target_include_directories(Vitracker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp
    ${rubberband_SOURCE_DIR})

target_compile_definitions(Vitracker PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    STMLIB_X86=1
    $<$<CXX_COMPILER_ID:MSVC>:_USE_MATH_DEFINES>
    $<$<CXX_COMPILER_ID:MSVC>:NOMINMAX>)

target_link_libraries(Vitracker PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# RubberBand uses vDSP on macOS (requires Accelerate framework)
if(APPLE)
    target_link_libraries(Vitracker PRIVATE "-framework Accelerate")
endif()

# Bundle DX7 preset sysex files as resources
file(GLOB DX7_SYSEX_FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/dx7/*.syx")
juce_add_bundle_resources_directory(Vitracker "${CMAKE_CURRENT_SOURCE_DIR}/resources/dx7")

# Unit Tests
enable_testing()

add_executable(DX7InstrumentTest
    tests/DX7InstrumentTest.cpp
    src/audio/DX7Instrument.cpp
    ${DSP_SOURCES}
)

target_include_directories(DX7InstrumentTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp
)

target_compile_definitions(DX7InstrumentTest PRIVATE
    STMLIB_X86=1
    $<$<CXX_COMPILER_ID:MSVC>:_USE_MATH_DEFINES>
    $<$<CXX_COMPILER_ID:MSVC>:NOMINMAX>
)

target_link_libraries(DX7InstrumentTest PRIVATE
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(DX7InstrumentTest)
