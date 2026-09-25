# Auxid: The Rigid C++ Platform.
#
# Copyright (C) 2026 I-A-S (ias@iasoft.dev)
# Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
#
# This source code is licensed under the Apache License, Version 2.0.
# A copy of this license is included in the LICENSE file at the root of this project,
# and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

set(AUXID_BENCH_NOEXC_SRC
    "cpp/main.cpp"

    "cpp/containers/hash_map_shared.hpp"
    "cpp/containers/hash_map_vs_unordered_map.cpp"
    "cpp/core/result_vs_exceptions_shared.hpp"
    "cpp/core/result_vs_exceptions.cpp"
)

execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE AUXID_BENCH_GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT AUXID_BENCH_GIT_COMMIT)
    set(AUXID_BENCH_GIT_COMMIT "unknown")
endif()
string(SUBSTRING "${AUXID_BENCH_GIT_COMMIT}" 0 7 AUXID_BENCH_COMMIT_SHORT)

add_executable(AuxidBench ${AUXID_BENCH_NOEXC_SRC})
set_target_properties(AuxidBench PROPERTIES CXX_SCAN_FOR_MODULES ON)
target_compile_definitions(AuxidBench PRIVATE
    AUXID_BENCH_COMMIT="${AUXID_BENCH_COMMIT_SHORT}"
)

add_library(auxid_bench_throw_baseline OBJECT
    "cpp/core/exception_baseline.cpp"
)
set_target_properties(auxid_bench_throw_baseline PROPERTIES
    CXX_SCAN_FOR_MODULES OFF
)
target_link_libraries(auxid_bench_throw_baseline PRIVATE benchmark::benchmark)

if(MSVC)
    target_compile_options(auxid_bench_throw_baseline PRIVATE /EHsc)
else()
    target_compile_options(auxid_bench_throw_baseline PRIVATE -fexceptions)
endif()

target_link_libraries(AuxidBench
    PRIVATE
        libauxid
        benchmark::benchmark
        auxid_bench_throw_baseline
)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
    target_link_libraries(AuxidBench PRIVATE stdc++exp)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(AuxidBench PRIVATE
        -Wno-deprecated-declarations
    )
endif()

get_target_property(_auxid_bench_compile_options AuxidBench COMPILE_OPTIONS)
if(_auxid_bench_compile_options)
    target_compile_options(auxid_bench_throw_baseline PRIVATE ${_auxid_bench_compile_options})
endif()

include(${CMAKE_SOURCE_DIR}/cmake/auxid_msvc_pgo.cmake)
auxid_msvc_pgo_target(libauxid)
auxid_msvc_pgo_target(AuxidBench)
auxid_msvc_pgo_target(auxid_bench_throw_baseline)

if(AUXID_MSVC_PGO_PHASE STREQUAL "INSTRUMENT")
    # Curated subset: every benchmark function runs once at the smallest size
    # point(s). Skips /32768 and /65536 Range points (O(n^2) key generation
    # under instrumentation is the dominant cost) and the /16384 Args
    # workload. Branch/edge profile data is preserved for every code path.
    set(_auxid_pgo_train_filter "/(512|4096|1024/[148])$")

    add_custom_target(auxid_bench_pgo_train
        COMMAND "$<TARGET_FILE:AuxidBench>"
                --benchmark_min_time=1x
                --benchmark_filter=${_auxid_pgo_train_filter}
        DEPENDS AuxidBench
        VERBATIM
        COMMENT "Collecting MSVC PGO training data (curated small-N subset)"
    )
endif()
