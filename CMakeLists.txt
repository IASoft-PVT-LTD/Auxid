# Auxid: The Rigid C++ Platform.
#
# Copyright (C) 2026 I-A-S (ias@iasoft.dev)
# Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
#
# This source code is licensed under the Apache License, Version 2.0.
# A copy of this license is included in the LICENSE file at the root of this project,
# and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

cmake_minimum_required(VERSION 3.28)

project(Auxid VERSION 0.7.0 LANGUAGES C CXX)

enable_testing()

set(AUXID_ROOT "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "")

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(AUXID_IS_TOP_LEVEL ON)
else()
    set(AUXID_IS_TOP_LEVEL OFF)
endif()

include(cmake/auxid_setup_project.cmake)

auxid_setup_project()

option(Auxid_BUILD_TESTS "Build unit tests" ${AUXID_IS_TOP_LEVEL})
option(Auxid_BUILD_BENCHMARKS "Build microbenchmarks (requires Google Benchmark via FetchContent)" OFF)
option(Auxid_BUILD_SHARED "Build libauxid as a shared library" OFF)

if(Auxid_BUILD_SHARED AND EMSCRIPTEN)
    message(FATAL_ERROR
        "Auxid_BUILD_SHARED is not supported for Emscripten/WASM targets.")
endif()

if(EMSCRIPTEN)
    add_compile_options(
        -msimd128
        -pthread
    )

    add_link_options(
        -pthread
        -sPTHREAD_POOL_SIZE=4
        -sPROXY_TO_PTHREAD
        -sEXIT_RUNTIME=1
        -sALLOW_MEMORY_GROWTH=1
        -sINITIAL_MEMORY=64MB
    )
endif()

add_subdirectory(src)

if(AUXID_IS_TOP_LEVEL)
    include(cmake/auxid_install.cmake)
endif()

if(Auxid_BUILD_TESTS)
  add_subdirectory(tests)

  add_test(
    NAME RigidCppBanList
    COMMAND ${CMAKE_COMMAND}
            -DAUXID_SRC_DIR=${AUXID_ROOT}/src
            -P ${AUXID_ROOT}/cmake/auxid_lint_ban_list.cmake
  )
endif()

if(Auxid_BUILD_BENCHMARKS)
  include(cmake/auxid_fetch_google_benchmark.cmake)
  add_subdirectory(benchmarks)
endif()
