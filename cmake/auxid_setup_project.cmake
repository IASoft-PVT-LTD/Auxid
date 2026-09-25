# Auxid: The Rigid C++ Platform.
#
# Copyright (C) 2026 I-A-S (ias@iasoft.dev)
# Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
#
# This source code is licensed under the Apache License, Version 2.0.
# A copy of this license is included in the LICENSE file at the root of this project,
# and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

macro(auxid_setup_project)

    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(Clang|MSVC|GNU)$")
        message(FATAL_ERROR "Auxid requires Clang, Clang-CL, native MSVC, or GCC 15.2+. Current compiler detected: ${CMAKE_CXX_COMPILER_ID}")
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        message(FATAL_ERROR
            "Auxid does not support AppleClang (insufficient C++ module support). "
            "On macOS, install LLVM Clang via Homebrew (brew install llvm) and use the arm64-darwin preset.")
    endif()

    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_C_EXTENSIONS OFF)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

    if(WIN32)
        add_compile_definitions(NOMINMAX)
    endif()

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "amd64|x86_64|AMD64")
        set(AUXID_ARCH_X64 TRUE CACHE INTERNAL "")
        add_compile_definitions(AUXID_ARCH_X64=1)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        set(AUXID_ARCH_ARM64 TRUE CACHE INTERNAL "")
        add_compile_definitions(AUXID_ARCH_ARM64=1)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "wasm32|emscripten")
        set(AUXID_ARCH_WASM TRUE CACHE INTERNAL "")
        add_compile_definitions(AUXID_ARCH_WASM=1)
    else()
        message(WARNING "Auxid: Unrecognized architecture '${CMAKE_SYSTEM_PROCESSOR}'. Proceed with caution.")
    endif()

    if(AUXID_ARCH_X64)
        add_compile_definitions(AUXID_HAS_SSE2=1)
    endif()
    if(AUXID_ARCH_ARM64)
        add_compile_definitions(AUXID_HAS_NEON=1)
    endif()
    if(AUXID_ARCH_WASM)
        add_compile_definitions(AUXID_HAS_WASM_SIMD128=1)
    endif()

    option(AUXID_DISABLE_HASHMAP_SIMD "Force the scalar SwissTable probe path" OFF)
    if(AUXID_DISABLE_HASHMAP_SIMD)
        add_compile_definitions(AUXID_DISABLE_HASHMAP_SIMD=1)
    endif()

    if(EMSCRIPTEN OR AUXID_ARCH_WASM)
        set(AUXID_USE_SYSTEM_MALLOC TRUE CACHE INTERNAL "")
        message(STATUS "Auxid: WASM/Emscripten target detected - using system malloc instead of rpmalloc.")
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(THREADS_PREFER_PTHREAD_FLAG ON)
        find_package(Threads REQUIRED)
        add_compile_options(-pthread)
        add_link_options(-pthread)
    endif()

    include(${AUXID_ROOT}/cmake/auxid_sanitizers.cmake)
    auxid_apply_sanitizers()

endmacro()