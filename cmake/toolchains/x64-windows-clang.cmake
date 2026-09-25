# Auxid: The Rigid C++ Platform.
#
# Copyright (C) 2026 I-A-S (ias@iasoft.dev)
# Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
#
# This source code is licensed under the Apache License, Version 2.0.
# A copy of this license is included in the LICENSE file at the root of this project,
# and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_RC_COMPILER llvm-rc)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

set(triple x86_64-pc-windows-msvc)
set(CMAKE_C_COMPILER_TARGET ${triple})
set(CMAKE_CXX_COMPILER_TARGET ${triple})

set(CMAKE_LINKER lld-link)

string(APPEND CMAKE_C_FLAGS   " -mavx2 -mfma")
string(APPEND CMAKE_CXX_FLAGS " -mavx2 -mfma")

# Clang otherwise selects the numerically newest Windows Kit even when that
# installation is partial (for example, kernel-mode headers without UCRT).
# Select the newest complete SDK explicitly so standard C headers and link
# libraries always come from the same version.
file(GLOB _auxid_windows_sdk_candidates LIST_DIRECTORIES TRUE
    "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/Include/*")
list(SORT _auxid_windows_sdk_candidates COMPARE NATURAL ORDER DESCENDING)
foreach(_auxid_windows_sdk_include IN LISTS _auxid_windows_sdk_candidates)
    if(IS_DIRECTORY "${_auxid_windows_sdk_include}/ucrt"
       AND IS_DIRECTORY "${_auxid_windows_sdk_include}/shared"
       AND IS_DIRECTORY "${_auxid_windows_sdk_include}/um")
        get_filename_component(_auxid_windows_sdk_version "${_auxid_windows_sdk_include}" NAME)
        set(_auxid_windows_sdk_lib "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/Lib/${_auxid_windows_sdk_version}")
        if(NOT IS_DIRECTORY "${_auxid_windows_sdk_lib}/ucrt/x64"
           OR NOT IS_DIRECTORY "${_auxid_windows_sdk_lib}/um/x64")
            continue()
        endif()

        set(CMAKE_SYSTEM_VERSION "${_auxid_windows_sdk_version}")
        foreach(_auxid_windows_sdk_component ucrt shared um winrt cppwinrt)
            if(IS_DIRECTORY "${_auxid_windows_sdk_include}/${_auxid_windows_sdk_component}")
                string(APPEND CMAKE_C_FLAGS
                    " -isystem \"${_auxid_windows_sdk_include}/${_auxid_windows_sdk_component}\"")
                string(APPEND CMAKE_CXX_FLAGS
                    " -isystem \"${_auxid_windows_sdk_include}/${_auxid_windows_sdk_component}\"")
                # CMake preprocesses .rc files in a separate clang -> llvm-rc
                # pipeline, so C/C++ flags do not supply the Windows SDK headers
                # needed by resources such as axiom.rc.
                string(APPEND CMAKE_RC_FLAGS
                    " -I \"${_auxid_windows_sdk_include}/${_auxid_windows_sdk_component}\"")
            endif()
        endforeach()
        set(_auxid_windows_sdk_link_flags
            " -Xlinker \"/libpath:${_auxid_windows_sdk_lib}/ucrt/x64\" -Xlinker \"/libpath:${_auxid_windows_sdk_lib}/um/x64\"")
        string(APPEND CMAKE_EXE_LINKER_FLAGS "${_auxid_windows_sdk_link_flags}")
        string(APPEND CMAKE_SHARED_LINKER_FLAGS "${_auxid_windows_sdk_link_flags}")
        string(APPEND CMAKE_MODULE_LINKER_FLAGS "${_auxid_windows_sdk_link_flags}")
        message(STATUS "Auxid: Clang using complete Windows SDK ${_auxid_windows_sdk_version}")
        break()
    endif()
endforeach()

if(NOT _auxid_windows_sdk_version)
    message(FATAL_ERROR "Auxid: no complete Windows 10 SDK with UCRT and x64 libraries was found")
endif()
