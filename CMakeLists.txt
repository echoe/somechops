cmake_minimum_required(VERSION 3.22)
project(SomeChops VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Use local JUCE if available, otherwise fetch ---
set(JUCE_LOCAL_PATH "$ENV{HOME}/JUCE" CACHE PATH "Path to local JUCE installation")
if(EXISTS "${JUCE_LOCAL_PATH}/CMakeLists.txt")
    message(STATUS "Using local JUCE at ${JUCE_LOCAL_PATH}")
    add_subdirectory(${JUCE_LOCAL_PATH} ${CMAKE_BINARY_DIR}/JUCE)
else()
    message(STATUS "Local JUCE not found at ${JUCE_LOCAL_PATH}, fetching...")
    include(FetchContent)
    FetchContent_Declare(
        juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.13
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(juce)
endif()

juce_add_plugin(SomeChops
    COMPANY_NAME "echoe"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Ycmp
    PLUGIN_CODE Slcr
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "SomeChops")

juce_generate_juce_header(SomeChops)

target_sources(SomeChops PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/TransientDetector.cpp
    Source/DrumSampler.cpp
    Source/Sequencer.cpp
    Source/PresetManager.cpp
)

target_compile_definitions(SomeChops PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SomeChops PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
