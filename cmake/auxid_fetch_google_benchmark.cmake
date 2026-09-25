# Auxid: The Rigid C++ Platform.
#
# Copyright (C) 2026 I-A-S (ias@iasoft.dev)
# Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
#
# This source code is licensed under the Apache License, Version 2.0.
# A copy of this license is included in the LICENSE file at the root of this project,
# and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

include_guard(GLOBAL)

include(FetchContent)

set(BENCHMARK_ENABLE_TESTING      OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL      OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS  OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_WERROR       OFF CACHE BOOL "" FORCE)
set(BENCHMARK_INSTALL_DOCS        OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_EXCEPTIONS   ON  CACHE BOOL "" FORCE)
set(BENCHMARK_DOWNLOAD_DEPENDENCIES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    google_benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.9.4
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(google_benchmark)

foreach(_bm_target benchmark benchmark_main)
    if(TARGET ${_bm_target})
        target_compile_definitions(${_bm_target} PRIVATE
            $<$<CONFIG:Release>:NDEBUG>
            $<$<CONFIG:RelWithDebInfo>:NDEBUG>
            $<$<CONFIG:MinSizeRel>:NDEBUG>
        )
        if(MSVC)
            target_compile_options(${_bm_target} PRIVATE
                $<$<CONFIG:Release>:/O2;/Ob2>
                $<$<CONFIG:RelWithDebInfo>:/O2;/Ob1>
                $<$<CONFIG:MinSizeRel>:/O1;/Ob1>
            )
        endif()
    endif()
endforeach()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    foreach(_bm_target benchmark benchmark_main)
        if(TARGET ${_bm_target})
            target_compile_options(${_bm_target} PRIVATE
                -Wno-unknown-warning-option
            )
        endif()
    endforeach()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    foreach(_bm_target benchmark benchmark_main)
        if(TARGET ${_bm_target})
            target_compile_options(${_bm_target} PRIVATE
                -Wno-c2y-extensions
            )
        endif()
    endforeach()
endif()
