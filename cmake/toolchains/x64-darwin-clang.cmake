# Auxid: The Rigid C++ Platform.
#
# Copyright (C) 2026 I-A-S (ias@iasoft.dev)
# Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
#
# This source code is licensed under the Apache License, Version 2.0.
# A copy of this license is included in the LICENSE file at the root of this project,
# and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_OSX_ARCHITECTURES x86_64)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "Minimum supported macOS version")

set(AUXID_LLVM_ROOT "$ENV{AUXID_LLVM_ROOT}" CACHE PATH "LLVM installation root")
if(NOT AUXID_LLVM_ROOT)
    set(AUXID_LLVM_ROOT "/usr/local/opt/llvm" CACHE PATH "LLVM installation root" FORCE)
endif()

if(DEFINED ENV{OSXCROSS_TARGET_DIR} AND NOT "$ENV{OSXCROSS_TARGET_DIR}" STREQUAL "")
    set(_auxid_osxcross_target "$ENV{OSXCROSS_TARGET_DIR}")
    file(GLOB _auxid_darwin_sdks LIST_DIRECTORIES TRUE "${_auxid_osxcross_target}/SDK/MacOSX*.sdk")
    list(SORT _auxid_darwin_sdks COMPARE NATURAL ORDER DESCENDING)
    if(NOT _auxid_darwin_sdks)
        message(FATAL_ERROR "No macOS SDK found under ${_auxid_osxcross_target}/SDK")
    endif()
    list(GET _auxid_darwin_sdks 0 _auxid_darwin_sdk)
    set(CMAKE_OSX_SYSROOT "${_auxid_darwin_sdk}" CACHE PATH "macOS SDK" FORCE)
    set(CMAKE_FIND_ROOT_PATH "${CMAKE_OSX_SYSROOT}")
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
    execute_process(
        COMMAND "${_auxid_osxcross_target}/bin/o64-clang" -dumpmachine
        OUTPUT_VARIABLE _auxid_osxcross_triple
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY)
    set(CMAKE_C_COMPILER_TARGET "${_auxid_osxcross_triple}")
    set(CMAKE_CXX_COMPILER_TARGET "${_auxid_osxcross_triple}")
    set(CMAKE_OBJC_COMPILER_TARGET "${_auxid_osxcross_triple}")
    set(CMAKE_OBJCXX_COMPILER_TARGET "${_auxid_osxcross_triple}")
    set(_auxid_osxcross_ar "${_auxid_osxcross_target}/bin/${_auxid_osxcross_triple}-ar")
    set(_auxid_osxcross_ranlib "${_auxid_osxcross_target}/bin/${_auxid_osxcross_triple}-ranlib")
    set(CMAKE_AR "${_auxid_osxcross_ar}" CACHE FILEPATH "Darwin archive tool" FORCE)
    set(CMAKE_RANLIB "${_auxid_osxcross_ranlib}" CACHE FILEPATH "Darwin archive indexer" FORCE)
    set(CMAKE_C_COMPILER_AR "${_auxid_osxcross_ar}" CACHE FILEPATH "C archive tool" FORCE)
    set(CMAKE_CXX_COMPILER_AR "${_auxid_osxcross_ar}" CACHE FILEPATH "C++ archive tool" FORCE)
    set(CMAKE_C_COMPILER_RANLIB "${_auxid_osxcross_ranlib}" CACHE FILEPATH "C archive indexer" FORCE)
    set(CMAKE_CXX_COMPILER_RANLIB "${_auxid_osxcross_ranlib}" CACHE FILEPATH "C++ archive indexer" FORCE)
else()
    if(NOT EXISTS "${AUXID_LLVM_ROOT}/bin/clang++")
        message(FATAL_ERROR
            "Auxid on Intel macOS requires LLVM Clang. Set AUXID_LLVM_ROOT to its prefix "
            "or OSXCROSS_TARGET_DIR to an OSXCross target directory. Expected: "
            "${AUXID_LLVM_ROOT}/bin/clang++")
    endif()
    execute_process(
        COMMAND xcrun --sdk macosx --show-sdk-path
        OUTPUT_VARIABLE _auxid_darwin_sdk
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY)
    set(CMAKE_OSX_SYSROOT "${_auxid_darwin_sdk}" CACHE PATH "macOS SDK" FORCE)
endif()

find_program(_auxid_clang_scan_deps NAMES clang-scan-deps-21 clang-scan-deps)
if(_auxid_clang_scan_deps)
    set(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS "${_auxid_clang_scan_deps}" CACHE FILEPATH "Clang dependency scanner" FORCE)
endif()
