<div align="center">
  <img src="logo.png" alt="Auxid Logo" width="190" style="border-radius: 1.15rem;"/>
  <br/>

  <img src="https://img.shields.io/badge/license-Apache_2.0-darkblue.svg" alt="License"/>
  <img src="https://img.shields.io/badge/standard-C%2B%2B23-darkred.svg" alt="C++ Standard"/>
  <img src="https://img.shields.io/badge/compiler-MSVC | Clang | GCC-darkgreen.svg" alt="Compiler"/>
  <img src="https://img.shields.io/badge/platforms-Linux | Windows | macOS | WASM-darkslateblue.svg" alt="Platforms"/>

  <p style="padding-top: 0.2rem;">
    <b>Auxid: The Rigid C++ Platform.</b>
  </p>
</div>

## Overview

Auxid is a platform for building modern, high-performance C++ applications using [**Rigid C++**](https://github.com/I-A-S/Rigid-Cpp) principles, delivered as a **C++23 named module**.

[**Rigid C++**](https://github.com/I-A-S/Rigid-Cpp) keeps runtime strict and predictable; no exceptions, no RTTI, explicit allocators, and a two-tier error model (`Result<T>` for recoverable failures, `panic()` for bugs); while using C++23's compile-time toolkit (modules, concepts) instead of deep template metaprogramming. Under **the Pragmatic STL Pact**, LibAuxid composes with the standard library when layout and error handling already fit (`std::vector` via `StdAllocatorAdapter`, non-throwing `std::filesystem`, vocabulary types and ranges); where they don't; node-based maps, global heap ownership, throwing I/O; it ships DOD-native replacements.

## Highlights

- **Modules-first API.** A single `import auxid;` gives you everything; tests pull in `import auxid.test;` separately. No mega-headers, no precompiled-headers.
- **No exceptions, ever.** `libauxid` propagates `-fno-exceptions` / `/EHs-c-` to every consumer via `PUBLIC` compile options. Errors flow through `Result<T>` and `AU_TRY`.
- **Strong allocators.** Integrated [rpmalloc](https://github.com/mjansson/rpmalloc) thread-caching heap, plus an `ArenaAllocator` for scoped bump-allocation. Both satisfy a single `memory::AllocatorType` concept, and `StdAllocatorAdapter` plugs them into standard containers.
- **Foreign-thread safe.** `HeapAllocator` works from threads Auxid never touched (embedded-engine callbacks, OS thread pools): rpmalloc lazily creates the thread heap on first allocation and finalizes it on thread exit; cross-thread frees route through atomic deferred lists. The thread guards remain the normal path — they also own the per-thread logger.
- **STL interop built in.** `Vec<T>` is `std::vector<T, StdAllocatorAdapter<T, A>>`, `Pair` / `Span` are direct std aliases, `Result<T, E>` round-trips with `std::expected`, `Option<T>` round-trips with `std::optional`, and `auxid::filesystem` wraps `std::filesystem` with non-throwing `Result<T>` returns.
- **Cache-friendly custom containers.** SwissTable-style `HashMap` / `HashSet` (SIMD 16-slot group probing with SSE2/NEON/WASM SIMD128, triangular probing, per-instance random seed) over a dense entry array, `String` with little-endian SSO, `CompactVec<T>` (u32 index) and `TinyVec<T>` (u16 index), lock-free `SpscQueue<T, N>`.
- **Standard-algorithm friendly iterators.** `Vec`, `String`, `StringView`, and `Span` model `std::contiguous_iterator` and `std::ranges::contiguous_range`; `StringView` is a `std::ranges::borrowed_range`. `std::ranges::sort` on a `Vec<i32>` or a `String` Just Works!
- **Lightweight smart pointers.** `Box<T>` (allocator-aware `unique_ptr`), `Arc<T>` (atomic shared), `IntrusiveArc<T>` over a `RefCounted` base.
- **The OS Foundation Layer.** One POSIX-flavored, IASoft-owned OS contract for every platform ([docs/OS-CONTRACT.md](docs/OS-CONTRACT.md)): time, runtime environment, local IPC (pipes, channels, single-instance locks), supervised processes with kill-on-close `LifetimeGroup`s, and dynamic libraries. Errors carry raw `errno`/`GetLastError` fidelity; what a platform cannot honestly express fails loudly (never a silent no-op).
- **Cross-platform.** x64 / ARM64 on Linux, Windows, and macOS (Apple Silicon), plus WebAssembly via Emscripten.

## Module map

| Module             | Interface                                                              | Implementation                                                                                                 | Contents                                                                                                       |
| ------------------ | ---------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------- |
| `auxid`            | [src/ixx/auxid.ixx](src/ixx/auxid.ixx)                       | —                                                                                                              | Umbrella; re-exports every module below (except `auxid.test`).                                           |
| `auxid.core`       | [src/ixx/auxid-core.ixx](src/ixx/auxid-core.ixx)             | [src/cpp/auxid-core.cpp](src/cpp/auxid-core.cpp)                                                             | Primitive aliases (`u8`..`u64`, `f32`/`f64`, `usize`), `Result<T, E>`, `panic`, `Mutex`.                       |
| `auxid.memory`     | [src/ixx/auxid-memory.ixx](src/ixx/auxid-memory.ixx)         | —                                                                                                              | `HeapAllocator` (rpmalloc), `ArenaAllocator`, `StdAllocatorAdapter`, `Box`, `Arc`, `IntrusiveArc`.             |
| `auxid.containers` | [src/ixx/auxid-containers.ixx](src/ixx/auxid-containers.ixx) | [src/cpp/auxid-containers.cpp](src/cpp/auxid-containers.cpp)                                                 | `String`/`StringView`, `Vec`/`CompactVec`/`TinyVec`, `HashMap`/`HashSet`, `SlotMap`, `Option`, `Pair`, `Span`, `SpscQueue`, hashing.|
| `auxid.thread`     | [src/ixx/auxid-thread.ixx](src/ixx/auxid-thread.ixx)         | [src/cpp/auxid-thread.cpp](src/cpp/auxid-thread.cpp)                                                         | `Thread`/`JThread` (native backend, spawn failure is a real `Err`), `LockGuard`, `ConditionVariable`, thread init, `Logger`. |
| `auxid.fs`         | [src/ixx/auxid-fs.ixx](src/ixx/auxid-fs.ixx)                 | —                                                                                                              | Non-throwing `std::filesystem` wrappers returning `Result<T>`.                                                 |
| `auxid.time`       | [src/ixx/auxid-time.ixx](src/ixx/auxid-time.ixx)             | [src/cpp/auxid-time.cpp](src/cpp/auxid-time.cpp)                                                             | OS layer: `monotonic_ns`, `wall_unix_ms`, `sleep_ms`.                                                          |
| `auxid.env`        | [src/ixx/auxid-env.ixx](src/ixx/auxid-env.ixx)               | [src/cpp/auxid-env.cpp](src/cpp/auxid-env.cpp)                                                               | OS layer: environment variables (UTF-8 API), `executable_path`, `standard_dir` per-OS directory table.         |
| `auxid.ipc`        | [src/ixx/auxid-ipc.ixx](src/ixx/auxid-ipc.ixx)               | [src/cpp/auxid-ipc.cpp](src/cpp/auxid-ipc.cpp)                                                               | OS layer: anonymous pipes, `Channel`/`ChannelListener` (named pipe / unix socket), `InstanceLock`.             |
| `auxid.proc`       | [src/ixx/auxid-proc.ixx](src/ixx/auxid-proc.ixx)             | [src/cpp/auxid-proc.cpp](src/cpp/auxid-proc.cpp)                                                             | OS layer: `spawn` with piped stdio, `Process` wait/status/terminate, `LifetimeGroup` kill-on-close supervision.|
| `auxid.dl`         | [src/ixx/auxid-dl.ixx](src/ixx/auxid-dl.ixx)                 | [src/cpp/auxid-dl.cpp](src/cpp/auxid-dl.cpp)                                                                 | OS layer: `Library` open / `symbol_as<T>` / close.                                                             |
| `auxid.test`       | [src/ixx/auxid-test.ixx](src/ixx/auxid-test.ixx)             | [src/cpp/auxid-test.cpp](src/cpp/auxid-test.cpp)                                                             | `Block`, `Runner`, `AutoRegister<T>` - tiny self-registering test framework.                                   |

## Quick start (CMake)

LibAuxid is meant to drop into an existing CMake project via `FetchContent`. **CMake 3.28+** is required (for `target_sources` `FILE_SET CXX_MODULES`).

```cmake
cmake_minimum_required(VERSION 3.29)
project(MyRigidEngine CXX)

include(FetchContent)

FetchContent_Declare(
  auxid
  GIT_REPOSITORY https://github.com/IASoft-PVT-LTD/Auxid.git
  GIT_TAG        v0.x.y  # ALWAYS pin a release tag; never track main in a product
)
FetchContent_MakeAvailable(auxid)

auxid_setup_project()

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE libauxid)
```

> [!NOTE]
> **Compiler requirements**
>
> Auxid hard-errors at configure time on anything other than **Clang**, **Clang-CL**, **native MSVC**, or **GCC 15.2+** (see [cmake/auxid_setup_project.cmake](cmake/auxid_setup_project.cmake)). Your consumer code must compile at C++23, and `libauxid` will propagate `-fno-exceptions` / `/EHs-c-` to it - this is a property of the library, not opt-in.

## Example

```cpp
#include <auxid/macros.hpp>

import auxid;

using namespace au;

auto load_config() -> Result<String>
{
    AU_TRY_VAR(cwd, filesystem::current_path());
    (void) cwd;
    return String("hello");
}

auto main() -> int
{
    auxid::MainThreadGuard _main_thread_guard;

    Vec<String> names;
    names.push_back(String("Rigid"));
    names.push_back(String("C++"));

    auto cfg = load_config();
    if (cfg.is_err())
        return 1;

    HashMap<String, i32> counts;
    counts.insert(String("ok"), 1);

    return 0;
}
```

Notes:

- `auxid::MainThreadGuard` initializes per-thread state (rpmalloc, logger). `Thread::create` / `JThread::create` install a `WorkerThreadGuard` automatically.
- `AU_TRY_VAR(name, expr)` (defined in [include/auxid/macros.hpp](include/auxid/macros.hpp)) propagates errors from any `Result<T>` and binds the success value to `name`. `AU_TRY` / `AU_TRY_DISCARD` exist for assignment to an existing variable and for discarding the value.
- `filesystem::current_path()` is the non-throwing wrapper from `auxid.fs`; the underlying `std::filesystem::path` is exposed as `filesystem::Path`.

## Repository layout

```
.
├── include/auxid/
│   ├── api.hpp                       # Public: AUXID_API
│   ├── macros.hpp                    # Public: AU_TRY*, platform/arch detection
├── src/
│   ├── cpp/*.cpp                     # Private module implementation units (module auxid.*;)
│   ├── c/vendor/rpmalloc/            # Vendored rpmalloc C source
│   ├── h/vendor/                     # Internal header-only vendors (rpmalloc, wyhash)
│   └── ixx/*.ixx                     # Public module interfaces (export module)
├── tests/
│   ├── CMakeLists.txt                # Builds the TestSuite executable
│   └── cpp/                          # Unit tests grouped by feature
├── cmake/
│   ├── auxid_setup_project.cmake     # C++23 + C11 + arch defines + WASM tweaks
│   └── toolchains/                   # Per-target toolchain files
├── CMakeLists.txt                    # Top-level; calls auxid_setup_project()
├── CMakePresets.json                 # Configure / build / test presets
└── .github/workflows/ci.yaml         # Linux x64, macOS ARM64, Windows x64, WASM CI
```

## Supported compilers & platforms

- **Compilers** (configure-time enforced): Clang, Clang-CL, native MSVC, GCC 15.2+. macOS requires **Homebrew LLVM Clang** (`brew install llvm`); AppleClang is not supported (C++ modules).
- **Architectures** (auto-detected, exposes `AUXID_ARCH_X64` / `AUXID_ARCH_ARM64` / `AUXID_ARCH_WASM`): x86_64, ARM64, WebAssembly (Emscripten forces `AUXID_USE_SYSTEM_MALLOC`).
- **Configure presets** from [CMakePresets.json](CMakePresets.json):

  | Preset                  | Generator               | Target                          |
  | ----------------------- | ----------------------- | ------------------------------- |
  | `x64-linux`             | Ninja Multi-Config      | Linux x64 (Clang)               |
  | `x64-linux-gcc`         | Ninja Multi-Config      | Linux x64 (GCC 15.2)            |
  | `arm64-linux`           | Ninja Multi-Config      | Linux ARM64 (Clang cross)       |
  | `arm64-linux-gcc`       | Ninja Multi-Config      | Linux ARM64 (GCC 15.2 cross)    |
  | `arm64-darwin`          | Ninja Multi-Config      | macOS ARM64 (Homebrew LLVM Clang) |
  | `x64-windows-clang`     | Ninja Multi-Config      | Windows x64 (Clang)             |
  | `x64-windows-clang-shared` | Ninja Multi-Config   | Windows x64 (Clang, shared `libauxid`) |
  | `arm64-windows-clang`   | Ninja Multi-Config      | Windows ARM64 (Clang cross)     |
  | `x64-windows-msvc`      | Ninja Multi-Config      | Windows x64 (MSVC)              |
  | `arm64-windows-msvc`    | Ninja Multi-Config      | Windows ARM64 (MSVC)            |
  | `x64-windows-mingw-gcc` | Ninja Multi-Config      | Windows x64 (MinGW GCC 15.2)    |
  | `x64-windows`           | Visual Studio 18 2026   | Windows x64 (VS / MSVC)         |
  | `arm64-windows`         | Visual Studio 18 2026   | Windows ARM64 (VS / MSVC)       |
  | `wasm32-emscripten`     | Ninja Multi-Config      | WebAssembly (Emscripten)        |

- **CI** ([.github/workflows/ci.yaml](.github/workflows/ci.yaml)) currently covers `x64-linux`, `arm64-darwin`, `x64-windows`, `x64-windows-clang-shared`, `x64-windows-mingw-gcc`, and `wasm32-emscripten` on every push and PR to `main`.

## Building & testing locally

By default, `libauxid` is built as a static library. 

- Pass `-DAuxid_BUILD_SHARED=ON` to build a shared library instead. 
- `Auxid_BUILD_SHARED` is not supported on Emscripten/WASM. 
- FetchContent consumers set the cache variable before `FetchContent_MakeAvailable(auxid)`.
- Shared builds define `AUXID_SHARED_BUILD` for consumers (import) and `AUXID_BUILDING_LIBRARY` when compiling `libauxid` itself (export). 
- Out-of-line public symbols use the `AUXID_API` macro from [include/auxid/api.hpp](include/auxid/api.hpp).
- On Windows, `libauxid` also enables `WINDOWS_EXPORT_ALL_SYMBOLS` so C++ module interface symbols (templates, inline methods) remain linkable from the import library; ELF shared builds use hidden visibility with explicit `AUXID_API` exports instead.

On macOS (Apple Silicon), install LLVM and Ninja first:

```bash
brew install llvm ninja
cmake --preset arm64-darwin
cmake --build --preset arm64-darwin --config Release
ctest --preset arm64-darwin --output-on-failure
```

Other platforms:

```bash
cmake --preset x64-windows-clang
cmake --build --preset x64-windows-clang --config Release
ctest --preset x64-windows-clang --output-on-failure
```

Shared library example:

```bash
cmake --preset x64-windows-clang-shared
cmake --build --preset x64-windows-clang-shared --config Release
ctest --preset x64-windows-clang-shared --output-on-failure
```

Tests are wired into a single `TestSuite` executable (see [tests/CMakeLists.txt](tests/CMakeLists.txt)); every translation unit under [tests/cpp/](tests/cpp/) self-registers via `test::AutoRegister<BlockType>`.

## The ecosystem

Explore the Auxid Ecosystem!

| Name | Description | Repository |
|------|-------------|------------|
| **LibAuxid** | Custom template library and core platform | [Auxid (This Repo)](https://github.com/IASoft-PVT-LTD/Auxid) |
| **Project Template** | Scaffold for new Auxid projects (CMake Presets Included) | [Auxid-Project-Template](https://github.com/IASoft-PVT-LTD/Auxid-Project-Template) |

## License

Copyright © 2026 IASoft (PVT) LTD.

Licensed under the [Apache License, Version 2.0](https://www.apache.org/licenses/LICENSE-2.0). See [LICENSE](LICENSE).

Vendored third-party components ([rpmalloc](https://github.com/mjansson/rpmalloc), [wyhash](https://github.com/wangyi-fudan/wyhash)) retain their own licenses, recorded in their source headers.
