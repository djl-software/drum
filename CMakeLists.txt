cmake_minimum_required(VERSION 3.22)
project(DJLDrum VERSION 0.16.20 LANGUAGES C CXX)
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
option(DRUM_BUILD_STANDALONE "Build the optional standalone development app" OFF)
set(drum_formats VST3)
if(DRUM_BUILD_STANDALONE)
  list(APPEND drum_formats Standalone)
endif()
juce_add_plugin(DJLDrum
  COMPANY_NAME "DJ Leslieson" BUNDLE_ID "com.djleslieson.drum"
  PLUGIN_MANUFACTURER_CODE DjlS PLUGIN_CODE DjDr
  FORMATS ${drum_formats} PRODUCT_NAME "DJL Drum"
  VST3_CATEGORIES Instrument Drum
  IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  HARDENED_RUNTIME_ENABLED TRUE)
juce_add_binary_data(DJLDrumAssets NAMESPACE DJLAssets HEADER_NAME DJLAssets.h
  SOURCES assets/DJLLogoWhite.png)
target_sources(DJLDrum PRIVATE src/PortableFiles.cpp src/GlobalKitStore.cpp src/PluginProcessor.cpp src/PluginEditor.cpp src/KitLibrary.cpp src/SampleEngine.cpp src/SoundEditor.cpp src/WaveformData.cpp src/EnvelopeEditor.cpp src/TonalAnalysis.cpp src/EqualizerEditor.cpp src/EffectRack.cpp src/BuiltinEffects.cpp src/ChainEditor.cpp src/LiveBrowserBridge.cpp)
target_compile_definitions(DJLDrum PUBLIC
  DRUM_VERSION_STRING="${PROJECT_VERSION}"
  JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(DJLDrum PRIVATE DJLDrumAssets juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags)
if(WIN32)
  target_link_libraries(DJLDrum PUBLIC comctl32)
endif()
if(BUILD_TESTING)
  juce_add_console_app(drum_tests PRODUCT_NAME "Drum Processor Tests")
  target_sources(drum_tests PRIVATE tests/PluginTests.cpp tests/ManagementTests.cpp tests/PlaybackTests.cpp tests/ControlTests.cpp tests/EditingTests.cpp tests/WaveformTests.cpp tests/PreviewTests.cpp tests/ChannelAttackTests.cpp tests/EnvelopeTests.cpp tests/EqualizerTests.cpp tests/PadTests.cpp tests/ChainTests.cpp tests/BuiltinTests.cpp tests/TrackLibraryTests.cpp tests/PatternTests.cpp)
  target_include_directories(drum_tests PRIVATE src)
  target_link_libraries(drum_tests PRIVATE DJLDrum juce::juce_audio_utils juce::juce_dsp)
  target_compile_definitions(drum_tests PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1)
  add_test(NAME drum_processor COMMAND drum_tests)
  add_dependencies(drum_tests DJLDrum_VST3)
  if(APPLE)
    set(drum_vst3_path "$<TARGET_BUNDLE_DIR:DJLDrum_VST3>")
  else()
    set(drum_vst3_path "$<TARGET_FILE_DIR:DJLDrum_VST3>/../..")
  endif()
  add_test(NAME vst3_sample_playback COMMAND drum_tests --vst3 "${drum_vst3_path}")
  juce_add_plugin(DrumChainTestEffect COMPANY_NAME "DJL Tests" PLUGIN_MANUFACTURER_CODE DjlS
    PLUGIN_CODE DjTf FORMATS VST3 PRODUCT_NAME "Drum Chain Test Effect"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
  target_sources(DrumChainTestEffect PRIVATE tests/ChainTestEffect.cpp)
  target_link_libraries(DrumChainTestEffect PRIVATE juce::juce_audio_processors juce::juce_recommended_config_flags)
  target_compile_definitions(DrumChainTestEffect PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
  add_dependencies(drum_tests DrumChainTestEffect_VST3)
  if(APPLE)
    set(drum_test_fx "$<TARGET_BUNDLE_DIR:DrumChainTestEffect_VST3>")
  else()
    set(drum_test_fx "$<TARGET_FILE_DIR:DrumChainTestEffect_VST3>/../..")
  endif()
  add_test(NAME vst3_instrument_chains COMMAND drum_tests --chains "${drum_test_fx}" "${drum_vst3_path}")
  if(WIN32)
    set(drum_native_test_host "${CMAKE_CURRENT_BINARY_DIR}/Ableton Live Drum Test.exe")
    add_custom_command(TARGET drum_tests POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:drum_tests>" "${drum_native_test_host}")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/native-browser-tests")
    add_test(NAME native_browser_target COMMAND "${drum_native_test_host}" --chains "${drum_test_fx}" "${drum_vst3_path}")
    set_tests_properties(native_browser_target PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/native-browser-tests")
  endif()

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
