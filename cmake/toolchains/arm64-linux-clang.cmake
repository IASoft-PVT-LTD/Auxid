# Auxid: The Rigid C++ Platform.
#
# Copyright (C) 2026 I-A-S (ias@iasoft.dev)
# Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
#
# This source code is licensed under the Apache License, Version 2.0.
# A copy of this license is included in the LICENSE file at the root of this project,
# and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(_auxid_triple aarch64-linux-gnu)
set(CMAKE_C_COMPILER_TARGET ${_auxid_triple})
set(CMAKE_CXX_COMPILER_TARGET ${_auxid_triple})

set(CMAKE_SYSROOT "/usr/${_auxid_triple}")
set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
set(CMAKE_LIBRARY_ARCHITECTURE ${_auxid_triple})

string(APPEND CMAKE_C_FLAGS " -march=armv8-a+simd --sysroot=${CMAKE_SYSROOT} --gcc-toolchain=/usr")
string(APPEND CMAKE_CXX_FLAGS " -march=armv8-a+simd --sysroot=${CMAKE_SYSROOT} --gcc-toolchain=/usr")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
