# Auxid: The Rigid C++ Platform.
#
# Copyright (C) 2026 I-A-S (ias@iasoft.dev)
# Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
#
# This source code is licensed under the Apache License, Version 2.0.
# A copy of this license is included in the LICENSE file at the root of this project,
# and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

include_guard(GLOBAL)

set(AUXID_SANITIZER "OFF" CACHE STRING
    "Enable sanitizer instrumentation: OFF, ASAN, UBSAN, TSAN, or MSAN")
set_property(CACHE AUXID_SANITIZER PROPERTY STRINGS OFF ASAN UBSAN TSAN MSAN)

set(AUXID_MSAN_LIBCXX_DIR "" CACHE PATH
    "Install prefix of an MSan-instrumented libc++/libc++abi (required for AUXID_SANITIZER=MSAN)")

function(auxid_apply_sanitizers)
    if(NOT AUXID_SANITIZER OR AUXID_SANITIZER STREQUAL "OFF")
        return()
    endif()

    if(MSVC)
        message(FATAL_ERROR "AUXID_SANITIZER is not supported with MSVC")
    endif()

    if(EMSCRIPTEN)
        message(FATAL_ERROR "AUXID_SANITIZER is not supported on Emscripten")
    endif()

    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(Clang|GNU)$")
        message(FATAL_ERROR
            "AUXID_SANITIZER requires Clang or GCC (got '${CMAKE_CXX_COMPILER_ID}')")
    endif()

    if(AUXID_SANITIZER STREQUAL "ASAN")
        set(_auxid_sanitize_flags "-fsanitize=address" "-fno-omit-frame-pointer" "-g")
        if(NOT AUXID_USE_SYSTEM_MALLOC)
            set(AUXID_USE_SYSTEM_MALLOC TRUE CACHE BOOL "Use system malloc instead of rpmalloc" FORCE)
            message(STATUS "Auxid: AddressSanitizer enabled - using system malloc instead of rpmalloc.")
        endif()
    elseif(AUXID_SANITIZER STREQUAL "UBSAN")
        set(_auxid_sanitize_flags "-fsanitize=undefined" "-fno-sanitize-recover=all" "-g")
    elseif(AUXID_SANITIZER STREQUAL "TSAN")
        set(_auxid_sanitize_flags "-fsanitize=thread" "-fno-omit-frame-pointer" "-g")
        if(NOT AUXID_USE_SYSTEM_MALLOC)
            set(AUXID_USE_SYSTEM_MALLOC TRUE CACHE BOOL "Use system malloc instead of rpmalloc" FORCE)
            message(STATUS "Auxid: ThreadSanitizer enabled - using system malloc instead of rpmalloc.")
        endif()
    elseif(AUXID_SANITIZER STREQUAL "MSAN")
        if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            message(FATAL_ERROR "AUXID_SANITIZER=MSAN requires Clang")
        endif()
        if(NOT AUXID_MSAN_LIBCXX_DIR OR NOT EXISTS "${AUXID_MSAN_LIBCXX_DIR}/include/c++/v1")
            message(FATAL_ERROR
                "AUXID_SANITIZER=MSAN requires AUXID_MSAN_LIBCXX_DIR to point at an "
                "MSan-instrumented libc++ install prefix "
                "(got '${AUXID_MSAN_LIBCXX_DIR}'). Every C++ object linked into the "
                "process must be instrumented, including the standard library.")
        endif()
        set(_auxid_sanitize_flags
            "-fsanitize=memory" "-fsanitize-memory-track-origins=2"
            "-fno-omit-frame-pointer" "-g")
        # -nostdinc++ plus the explicit -isystem fully select the instrumented
        # libc++ headers; adding -stdlib=libc++ as well would trip
        # -Werror,-Wunused-command-line-argument in dependencies (AWS-LC).
        add_compile_options(
            "$<$<COMPILE_LANGUAGE:CXX>:-nostdinc++;-isystem${AUXID_MSAN_LIBCXX_DIR}/include/c++/v1>")
        add_link_options(
            "-stdlib=libc++"
            "-L${AUXID_MSAN_LIBCXX_DIR}/lib"
            "-Wl,-rpath,${AUXID_MSAN_LIBCXX_DIR}/lib")
        if(NOT AUXID_USE_SYSTEM_MALLOC)
            set(AUXID_USE_SYSTEM_MALLOC TRUE CACHE BOOL "Use system malloc instead of rpmalloc" FORCE)
            message(STATUS "Auxid: MemorySanitizer enabled - using system malloc instead of rpmalloc.")
        endif()
    else()
        message(FATAL_ERROR
            "AUXID_SANITIZER must be OFF, ASAN, UBSAN, TSAN, or MSAN (got '${AUXID_SANITIZER}')")
    endif()

    add_compile_options(${_auxid_sanitize_flags})
    add_link_options(${_auxid_sanitize_flags})

    message(STATUS "Auxid: Sanitizer ${AUXID_SANITIZER} enabled")
endfunction()
