cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14" CACHE STRING "Minimum OS X deployment version. Used only for macOS")

set(CMAKE_POLICY_DEFAULT_CMP0063 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

project(sherpa-onnx)

set(SHERPA_ONNX_VERSION "1.10.38")

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
  cmake_policy(SET CMP0135 NEW)
endif()

# Core options - most features disabled by default
option(BUILD_SHARED_LIBS "Whether to build shared libraries" OFF)
option(SHERPA_ONNX_ENABLE_PORTAUDIO "Whether to build with portaudio" ON)
option(SHERPA_ONNX_ENABLE_BINARY "Whether to build binaries" ON)
option(SHERPA_ONNX_LINK_LIBSTDCPP_STATICALLY "True to link libstdc++ statically. Used only when BUILD_SHARED_LIBS is OFF on Linux" ON)
option(SHERPA_ONNX_USE_PRE_INSTALLED_ONNXRUNTIME_IF_AVAILABLE "True to use pre-installed onnxruntime if available" ON)

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

# Output build configuration
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS}")
message(STATUS "SHERPA_ONNX_ENABLE_PORTAUDIO ${SHERPA_ONNX_ENABLE_PORTAUDIO}")
message(STATUS "SHERPA_ONNX_ENABLE_BINARY ${SHERPA_ONNX_ENABLE_BINARY}")

if(BUILD_SHARED_LIBS)
  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17 CACHE STRING "The C++ version to be used.")
endif()
set(CMAKE_CXX_EXTENSIONS OFF)
message(STATUS "C++ Standard version: ${CMAKE_CXX_STANDARD}")

if(WIN32)
  add_definitions(-DNOMINMAX) # Otherwise, std::max() and std::min() won't work
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

if(APPLE)
  # Add Foundation framework for Objective-C runtime
  find_library(FOUNDATION_LIBRARY Foundation)
  if(NOT FOUNDATION_LIBRARY)
    message(FATAL_ERROR "Foundation framework not found")
  endif()
  
  # Add CoreML framework
  find_library(CORE_ML_LIBRARY CoreML)
  if(NOT CORE_ML_LIBRARY)
    message(FATAL_ERROR "CoreML framework not found")
  endif()

  set(SHERPA_ONNX_PKG_CONFIG_EXTRA_LIBS 
    "-framework Foundation -framework CoreML -lc++"
  )
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
    set(SHERPA_ONNX_PKG_CONFIG_EXTRA_LIBS 
      "-framework Foundation -framework CoreML -lc++"
    )
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
