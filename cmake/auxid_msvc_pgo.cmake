# Auxid: The Rigid C++ Platform.
#
# Copyright (C) 2026 I-A-S (ias@iasoft.dev)
# Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
#
# This source code is licensed under the Apache License, Version 2.0.
# A copy of this license is included in the LICENSE file at the root of this project,
# and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

include_guard(GLOBAL)

set(AUXID_MSVC_PGO_PHASE "OFF" CACHE STRING
    "MSVC PGO phase for benchmark presets: OFF, INSTRUMENT, or USE")
set_property(CACHE AUXID_MSVC_PGO_PHASE PROPERTY STRINGS OFF INSTRUMENT USE)

function(auxid_msvc_pgo_target target)
    if(NOT MSVC OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        return()
    endif()
    if(NOT AUXID_MSVC_PGO_PHASE OR AUXID_MSVC_PGO_PHASE STREQUAL "OFF")
        return()
    endif()

    if(AUXID_MSVC_PGO_PHASE STREQUAL "INSTRUMENT")
        set(_auxid_pgo_link "/LTCG:PGINSTRUMENT")
    elseif(AUXID_MSVC_PGO_PHASE STREQUAL "USE")
        set(_auxid_pgo_link "/LTCG:PGOPTIMIZE")
    else()
        message(FATAL_ERROR
            "AUXID_MSVC_PGO_PHASE must be OFF, INSTRUMENT, or USE (got '${AUXID_MSVC_PGO_PHASE}')")
    endif()

    target_compile_options(${target} PRIVATE
        "$<$<CONFIG:Release>:/GL>"
    )
    target_link_options(${target} PRIVATE
        "$<$<CONFIG:Release>:${_auxid_pgo_link}>"
    )
endfunction()
