# Stereo Bucket Flanger — BBD stereo flanger model inspired by the Roland SBF-325 (AU / VST3 / CLAP + harness)
cmake_minimum_required(VERSION 3.22)
project(StereoBucketFlanger VERSION 0.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  # universal by default; -DSBF_ARCHS=arm64 for a faster native-only dev build
  set(SBF_ARCHS "arm64;x86_64" CACHE STRING "macOS architectures to build")
  set(CMAKE_OSX_ARCHITECTURES "${SBF_ARCHS}" CACHE STRING "" FORCE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

option(SBF_BUILD_PLUGINS "Build the JUCE plugin targets" ON)
option(SBF_BUILD_CLAP "Build the CLAP format via clap-juce-extensions" ON)

# ---- JUCE-free targets: unit tests + standalone renderer ------------------
add_executable(sbf_tests tests/UnitTests.cpp)
add_executable(sbf_render tools/Render.cpp)
foreach(t sbf_tests sbf_render)
  if(NOT MSVC)
    target_compile_options(${t} PRIVATE -Wall -Wextra)
  endif()
endforeach()
enable_testing()
add_test(NAME unit COMMAND sbf_tests)
add_test(NAME measurements
  COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
          python3 ${CMAKE_SOURCE_DIR}/measure/run_all.py $<TARGET_FILE:sbf_render> --out ${CMAKE_BINARY_DIR}/results)

if(NOT SBF_BUILD_PLUGINS)
  return()
endif()

# ---- JUCE: local checkout when present, otherwise fetch 8.0.6 --------------
include(FetchContent)
if(DEFINED SBF_JUCE_PATH AND EXISTS "${SBF_JUCE_PATH}/CMakeLists.txt")
  add_subdirectory("${SBF_JUCE_PATH}" JUCE)
elseif(EXISTS "$ENV{HOME}/JUCE/CMakeLists.txt")
  message(STATUS "Using local JUCE at $ENV{HOME}/JUCE")
  add_subdirectory("$ENV{HOME}/JUCE" JUCE)
else()
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.6 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

set(SBF_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND SBF_FORMATS AU)
endif()

if(SBF_BUILD_CLAP)
  FetchContent_Declare(clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG main          # tagged releases predate JUCE 8; main tracks it
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(clap-juce-extensions)
endif()

juce_add_plugin(StereoBucketFlanger
  PRODUCT_NAME "Stereo Bucket Flanger"
  COMPANY_NAME "Renky Denk"
  BUNDLE_ID com.renkydenk.stereobucketflanger
  PLUGIN_MANUFACTURER_CODE Rnky
  PLUGIN_CODE Sbf3
  FORMATS ${SBF_FORMATS}
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(StereoBucketFlanger PRIVATE src/plugin/PluginProcessor.cpp)
target_compile_definitions(StereoBucketFlanger PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(StereoBucketFlanger PRIVATE
  juce::juce_audio_utils
  PUBLIC
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags)

if(SBF_BUILD_CLAP)
  clap_juce_extensions_plugin(TARGET StereoBucketFlanger
    CLAP_ID "com.renkydenk.stereobucketflanger"
    CLAP_FEATURES audio-effect flanger chorus stereo)
endif()

# ---- UI snapshot tool (renders the editor to PNG, no host needed) -----------
juce_add_console_app(sbf_snapshot PRODUCT_NAME "sbf_snapshot")
target_sources(sbf_snapshot PRIVATE tools/Snapshot.cpp src/plugin/PluginProcessor.cpp)
target_compile_definitions(sbf_snapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(sbf_snapshot PRIVATE juce::juce_audio_utils juce::juce_gui_extra
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
