# Auxid: The Rigid C++ Platform.
#
# Copyright (C) 2026 I-A-S (ias@iasoft.dev)
# Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
#
# This source code is licensed under the Apache License, Version 2.0.
# A copy of this license is included in the LICENSE file at the root of this project,
# and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.


set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_RC_COMPILER llvm-rc)

set(CMAKE_LINKER lld-link)

set(triple arm64-pc-windows-msvc)
set(CMAKE_C_COMPILER_TARGET   ${triple})
set(CMAKE_CXX_COMPILER_TARGET ${triple})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

string(APPEND CMAKE_C_FLAGS   " --target=arm64-pc-windows-msvc")
string(APPEND CMAKE_CXX_FLAGS " --target=arm64-pc-windows-msvc")
