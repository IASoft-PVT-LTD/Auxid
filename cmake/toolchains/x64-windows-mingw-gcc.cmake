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

string(APPEND CMAKE_C_FLAGS " -mavx2 -mfma")
string(APPEND CMAKE_CXX_FLAGS " -mavx2 -mfma")

# Runtime libraries MUST stay dynamic on mingw-gcc. Static libgcc links
# emutls into the executable; emutls's first-touch allocates via calloc,
# which binds to rpmalloc's global override, whose thread-heap variable is
# itself emutls under mingw (GCC has no native PE TLS) -> infinite mutual
# recursion (stack-overflow SIGSEGV). Static winpthread livelocks the same
# way through pthread_once. Dynamic libgcc/winpthread resolve their calloc
# against ucrt inside the DLL, never entering rpmalloc.
#
# PATH hermeticity (Git-for-Windows/Strawberry/msys2 all ship divergent
# runtime DLLs; a mismatch dies at load with STATUS_ENTRYPOINT_NOT_FOUND) is
# achieved by copying the toolchain's libstdc++-6/libgcc_s_seh-1/
# libwinpthread-1 DLLs beside every executable instead — see
# tests/CMakeLists.txt.
