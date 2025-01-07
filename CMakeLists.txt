cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14" CACHE STRING "Minimum OS X deployment version. Used only for macOS")

set(CMAKE_POLICY_DEFAULT_CMP0063 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

project(sherpa-onnx)

# Remember to update
# ./CHANGELOG.md
# ./new-release.sh
set(SHERPA_ONNX_VERSION "1.10.38")

# Disable warning about
#
# "The DOWNLOAD_EXTRACT_TIMESTAMP option was not given and policy CMP0135 is
#  not set.
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
  cmake_policy(SET CMP0135 NEW)
endif()

# Core options - most features disabled by default
option(SHERPA_ONNX_ENABLE_PYTHON "Whether to build Python" OFF)
option(SHERPA_ONNX_ENABLE_TESTS "Whether to build tests" OFF)
option(SHERPA_ONNX_ENABLE_CHECK "Whether to build with assert" OFF)
option(BUILD_SHARED_LIBS "Whether to build shared libraries" OFF)
option(SHERPA_ONNX_ENABLE_PORTAUDIO "Whether to build with portaudio" ON)
option(SHERPA_ONNX_ENABLE_JNI "Whether to build JNI internface" OFF)
option(SHERPA_ONNX_ENABLE_C_API "Whether to build C API" OFF)
option(SHERPA_ONNX_ENABLE_WEBSOCKET "Whether to build webscoket server/client" OFF)
option(SHERPA_ONNX_ENABLE_GPU "Enable ONNX Runtime GPU support" OFF)
option(SHERPA_ONNX_ENABLE_DIRECTML "Enable ONNX Runtime DirectML support" OFF)
option(SHERPA_ONNX_ENABLE_WASM "Whether to enable WASM" OFF)
option(SHERPA_ONNX_ENABLE_TTS "Whether to build TTS related code" OFF)
option(SHERPA_ONNX_ENABLE_SPEAKER_DIARIZATION "Whether to build speaker diarization related code" OFF)
option(SHERPA_ONNX_ENABLE_BINARY "Whether to build binaries" ON)
option(SHERPA_ONNX_LINK_LIBSTDCPP_STATICALLY "True to link libstdc++ statically. Used only when BUILD_SHARED_LIBS is OFF on Linux" ON)
option(SHERPA_ONNX_USE_PRE_INSTALLED_ONNXRUNTIME_IF_AVAILABLE "True to use pre-installed onnxruntime if available" ON)
option(SHERPA_ONNX_ENABLE_SANITIZER "Whether to enable ubsan and asan" OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(NOT WIN32)
  set(CMAKE_SKIP_BUILD_RPATH FALSE)
  set(BUILD_RPATH_USE_ORIGIN TRUE)
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

if(NOT APPLE)
  set(SHERPA_ONNX_RPATH_ORIGIN "$ORIGIN")
else()
  set(SHERPA_ONNX_RPATH_ORIGIN "@loader_path")
endif()

if(NOT WIN32)
  set(CMAKE_INSTALL_RPATH ${SHERPA_ONNX_RPATH_ORIGIN})
  set(CMAKE_BUILD_RPATH ${SHERPA_ONNX_RPATH_ORIGIN})
endif()

if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "No CMAKE_BUILD_TYPE given, default to Release")
  set(CMAKE_BUILD_TYPE Release)
endif()

if(DEFINED ANDROID_ABI AND NOT SHERPA_ONNX_ENABLE_JNI)
  message(STATUS "Set SHERPA_ONNX_ENABLE_JNI to ON for Android")
  set(SHERPA_ONNX_ENABLE_JNI ON CACHE BOOL "" FORCE)
endif()

# Output build configuration
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS}")
message(STATUS "SHERPA_ONNX_ENABLE_PORTAUDIO ${SHERPA_ONNX_ENABLE_PORTAUDIO}")
message(STATUS "SHERPA_ONNX_ENABLE_BINARY ${SHERPA_ONNX_ENABLE_BINARY}")

if(BUILD_SHARED_LIBS OR SHERPA_ONNX_ENABLE_JNI)
  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

if(BUILD_SHARED_LIBS AND NOT CMAKE_SYSTEM_NAME STREQUAL iOS AND CMAKE_BUILD_TYPE STREQUAL Release)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo)
  if(ipo)
    message(STATUS "IPO is enabled")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
  else()
    message(STATUS "IPO is not available")
  endif()
endif()

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17 CACHE STRING "The C++ version to be used.")
endif()
set(CMAKE_CXX_EXTENSIONS OFF)
message(STATUS "C++ Standard version: ${CMAKE_CXX_STANDARD}")

include(CheckIncludeFileCXX)

if(UNIX AND NOT APPLE AND NOT SHERPA_ONNX_ENABLE_WASM AND NOT CMAKE_SYSTEM_NAME STREQUAL Android)
  check_include_file_cxx(alsa/asoundlib.h SHERPA_ONNX_HAS_ALSA)
  if(SHERPA_ONNX_HAS_ALSA)
    message(STATUS "With Alsa")
    add_definitions(-DSHERPA_ONNX_ENABLE_ALSA=1)
  else()
    message(WARNING "\
Could not find alsa/asoundlib.h !
We won't build sherpa-onnx-alsa
To fix that, please do:
  (1) sudo apt-get install alsa-utils libasound2-dev
  (2) rm -rf build
  (3) re-try
  ")
  endif()
endif()

check_include_file_cxx(cxxabi.h SHERPA_ONNX_HAVE_CXXABI_H)
check_include_file_cxx(execinfo.h SHERPA_ONNX_HAVE_EXECINFO_H)

if(WIN32)
  add_definitions(-DNOMINMAX) # Otherwise, std::max() and std::min() won't work
endif()

if(WIN32 AND MSVC)
  # disable various warnings for MSVC
  set(disabled_warnings
      /wd4244
      /wd4267
      /wd4305
      /wd4334
      /wd4800
      /wd4996
  )
  message(STATUS "Disabled warnings: ${disabled_warnings}")
  foreach(w IN LISTS disabled_warnings)
    string(APPEND CMAKE_CXX_FLAGS " ${w} ")
  endforeach()

  add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
  add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

if(NOT BUILD_SHARED_LIBS AND CMAKE_SYSTEM_NAME STREQUAL Linux)
  if(SHERPA_ONNX_LINK_LIBSTDCPP_STATICALLY)
    message(STATUS "Link libstdc++ statically")
    set(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc ")
  else()
    message(STATUS "Link libstdc++ dynamically")
  endif()
endif()

# Include essential dependencies only
include(kaldi-native-fbank)
include(kaldi-decoder)
include(onnxruntime)
include(simple-sentencepiece)

if(SHERPA_ONNX_ENABLE_PORTAUDIO AND SHERPA_ONNX_ENABLE_BINARY)
  include(portaudio)
endif()

add_subdirectory(sherpa-onnx)

message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")

if(NOT BUILD_SHARED_LIBS)
  if(APPLE)
    set(SHERPA_ONNX_PKG_CONFIG_EXTRA_LIBS "-lc++ -framework Foundation")
  endif()

  if(UNIX AND NOT APPLE)
    set(SHERPA_ONNX_PKG_CONFIG_EXTRA_LIBS "-lstdc++ -lm -pthread -ldl")
  endif()
endif()

if(NOT BUILD_SHARED_LIBS)
  configure_file(cmake/sherpa-onnx-static-no-tts.pc.in ${PROJECT_BINARY_DIR}/sherpa-onnx.pc @ONLY)
else()
  configure_file(cmake/sherpa-onnx-shared.pc.in ${PROJECT_BINARY_DIR}/sherpa-onnx.pc @ONLY)
endif()

install(
  FILES
    ${PROJECT_BINARY_DIR}/sherpa-onnx.pc
  DESTINATION
    ./
)
message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
