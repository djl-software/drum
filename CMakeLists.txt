cmake_minimum_required(VERSION 3.22)
project(DJLDrum VERSION 0.1.0 LANGUAGES C CXX)
if(APPLE)
  enable_language(OBJC OBJCXX)
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CTest)
if(DEFINED JUCE_SOURCE_DIR)
  add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
else()
  include(FetchContent)
  # Same pinned JUCE release as Meter.
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.12 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()
juce_add_plugin(DJLDrum
  COMPANY_NAME "DJ Leslieson" BUNDLE_ID "com.djleslieson.drum"
  PLUGIN_MANUFACTURER_CODE DjlS PLUGIN_CODE DjDr
  FORMATS VST3 Standalone PRODUCT_NAME "DJL Drum"
  IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  MICROPHONE_PERMISSION_ENABLED TRUE
  MICROPHONE_PERMISSION_TEXT "DJL Drum needs audio input access for standalone audio processing."
  HARDENED_RUNTIME_ENABLED TRUE
  HARDENED_RUNTIME_OPTIONS com.apple.security.device.audio-input)
juce_add_binary_data(DJLDrumAssets NAMESPACE DJLAssets HEADER_NAME DJLAssets.h
  SOURCES assets/DJLLogoWhite.png)
target_sources(DJLDrum PRIVATE src/PluginProcessor.cpp src/PluginEditor.cpp)
target_compile_definitions(DJLDrum PUBLIC
  DRUM_VERSION_STRING="${PROJECT_VERSION}"
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(DJLDrum PRIVATE DJLDrumAssets juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags)
if(BUILD_TESTING)
  juce_add_console_app(drum_tests PRODUCT_NAME "Drum Processor Tests")
  target_sources(drum_tests PRIVATE tests/PluginTests.cpp)
  target_include_directories(drum_tests PRIVATE src)
  target_link_libraries(drum_tests PRIVATE DJLDrum juce::juce_audio_utils juce::juce_dsp)
  add_test(NAME drum_processor COMMAND drum_tests)
endif()
# Keep Windows resource versions current after a project version change.
if(WIN32)
  set(drum_resource_targets DJLDrum)
  if(BUILD_TESTING)
    list(APPEND drum_resource_targets drum_tests)
  endif()
  foreach(drum_target IN LISTS drum_resource_targets)
    set(drum_info_dir "${CMAKE_CURRENT_BINARY_DIR}/${drum_target}_artefacts/JuceLibraryCode")
    add_custom_command(OUTPUT "${drum_info_dir}/${drum_target}_resources.rc" APPEND
      DEPENDS "${drum_info_dir}/Info.txt")
  endforeach()
endif()
