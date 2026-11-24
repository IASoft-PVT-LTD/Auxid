<div align="center">
  <img src="logo.png" alt="Auxid Logo" width="190" style="border-radius: 1.15rem;"/>
  <br/>

  <img src="https://img.shields.io/badge/license-Apache_2.0-darkblue.svg" alt="License"/>
  <img src="https://img.shields.io/badge/standard-C%2B%2B20-darkred.svg" alt="C++ Standard"/>
  <img src="https://img.shields.io/badge/compiler-MSVC | Clang | GCC-darkgreen.svg" alt="Compiler"/>
  <img src="https://img.shields.io/badge/platforms-Linux | Windows | macOS | WASM-darkslateblue.svg" alt="Platforms"/>

  <p style="padding-top: 0.2rem;">
    <b>Auxid: The Rigid C++ Platform.</b>
  </p>
</div>

Auxid is a C++20 library for writing programs the [Rigid C++](https://github.com/I-A-S/Rigid-Cpp) way: no exceptions, no RTTI, and explicit allocators. You get all of it from one `import auxid;`.

It keeps the parts of the standard library that already fit those rules and replaces the parts that don't. `Vec` is `std::vector` with an Auxid allocator, and `Span` and `Pair` are the standard types. Node-based maps, hidden global-heap use and throwing I/O are replaced. We call that split the Pragmatic STL Pact.

## Quick start

You need CMake 3.28 or newer and one of these compilers: Clang, Clang-CL, MSVC, or GCC 15.2+.

```cmake
cmake_minimum_required(VERSION 3.28)
project(my_app CXX)

include(FetchContent)

FetchContent_Declare(
  auxid
  GIT_REPOSITORY https://github.com/IASoft-PVT-LTD/Auxid.git
  GIT_TAG        v0.x.y  # pin a release tag, don't track main
)
FetchContent_MakeAvailable(auxid)

auxid_setup_project()

add_executable(my_app main.cpp)
set_target_properties(my_app PROPERTIES CXX_SCAN_FOR_MODULES ON)
target_link_libraries(my_app PRIVATE libauxid)
```

`main.cpp`, which finds the size of the largest file in a directory:

```cpp
#include <auxid/macros.hpp>

#include <new>
#include <vector>

import auxid;

using namespace au;

auto largest_file(const filesystem::Path &dir) -> Result<u64>
{
  u64 largest = 0;
  AU_TRY_VAR(it, filesystem::make_directory_iterator(dir));
  while (it != filesystem::DirectoryIterator{})
  {
    AU_TRY_VAR(is_file, filesystem::is_regular_file(it->path()));
    if (is_file)
    {
      AU_TRY_VAR(size, filesystem::file_size(it->path()));
      largest = size > largest ? size : largest;
    }
    AU_TRY_DISCARD(filesystem::advance(it));
  }
  return largest;
}

auto main() -> int
{
  auxid::MainThreadGuard guard;
  Logger &log = auxid::get_thread_logger();

  auto largest = largest_file(".");
  if (largest.is_err())
  {
    log.error("{}", largest.unwrap_err().describe());
    return 1;
  }
  log.info("largest file: {} bytes", largest.unwrap());
  return 0;
}
```

Nothing here throws. Each filesystem call returns a `Result`, and `AU_TRY_VAR` either gives you the value or returns the error to the caller. `MainThreadGuard` sets up the heap and the logger for the main thread. Threads made with `Thread::create` set themselves up.

The `<new>` and `<vector>` includes are for GCC, which needs them next to the import.

### Things to know before you link it

- `libauxid` passes `-fno-exceptions` and `-fno-rtti` (`/EHs-c-` and `/GR-` on MSVC) to every target that links it.
- Auxid builds as C++20 unless you ask for something newer. Auxid and your code must use the same standard, so set `CMAKE_CXX_STANDARD` before `FetchContent_MakeAvailable`.
- `auxid_setup_project()` turns CMake's module scanning off by default, which is why the target above turns it back on.
- AppleClang doesn't work. Use Homebrew LLVM on macOS.
- GCC 15.2 currently crashes when it compiles `libauxid` in Release. Debug builds are fine.

## What's in it

**Errors.** Functions that can fail return `Result<T>`. Bugs call `panic()`, which prints the location and aborts. `Result` converts to and from `std::expected` when your standard library has it (C++23).

**Memory.** The default heap is [rpmalloc](https://github.com/mjansson/rpmalloc), and it works from any thread, including ones Auxid didn't create. `ArenaAllocator` hands out memory from a buffer you own. `Box`, `Arc` and `IntrusiveArc` are the smart pointers.

**Containers.** `HashMap` and `HashSet` use SwissTable-style probing, with SSE2, NEON and WASM SIMD paths. `String` keeps short text inline. `CompactVec` and `TinyVec` use 32-bit and 16-bit sizes. There are also `SlotMap`, `Option` and a lock-free single-producer queue, `SpscQueue`. The iterators work with standard algorithms, so `std::ranges::sort` on a `Vec<i32>` or a `String` does what you'd expect.

**The operating system.** Time, environment variables, pipes and local sockets, single-instance locks, child processes, and dynamic libraries. The rules are the same on every platform and written down in [docs/OS-CONTRACT.md](docs/OS-CONTRACT.md). An operation the platform can't do returns an error.

### Arenas

An arena can't be copied. Containers share one through a handle, `AllocatorRef`:

```cpp
using ArenaRef = memory::AllocatorRef<memory::ArenaAllocator>;

alignas(16) static u8 buffer[4096];
memory::ArenaAllocator arena;
arena.init(buffer, sizeof(buffer));

{
  ArenaRef ref(arena);
  Vec<i32, ArenaRef> ids(ref);
  BasicString<ArenaRef> name(StringView("player"), ref);
  ids.push_back(1);
}
arena.clear();
```

Debug builds panic if you clear or destroy an arena while a handle to it is still alive. Handles stored inside the arena's own buffer don't count, so you can build objects in an arena and drop them all with `clear()`.

## Modules

`import auxid;` brings in every module below except `auxid.test`.

| Module | Contents |
| --- | --- |
| [`auxid.core`](src/ixx/auxid-core.ixx) | `u8` to `u64`, `f32`, `f64`, `usize`, `Result`, `panic`, `Mutex` |
| [`auxid.memory`](src/ixx/auxid-memory.ixx) | `HeapAllocator`, `ArenaAllocator`, `AllocatorRef`, `StdAllocatorAdapter`, `Box`, `Arc`, `IntrusiveArc` |
| [`auxid.containers`](src/ixx/auxid-containers.ixx) | Strings, vectors, hash containers, `SlotMap`, `Option`, `Pair`, `Span`, `SpscQueue`, `Error` |
| [`auxid.thread`](src/ixx/auxid-thread.ixx) | `Thread`, `JThread`, `LockGuard`, `ConditionVariable`, `Logger`, thread setup |
| [`auxid.fs`](src/ixx/auxid-fs.ixx) | `std::filesystem` calls that return `Result` |
| [`auxid.time`](src/ixx/auxid-time.ixx) | `monotonic_ns`, `wall_unix_ms`, `sleep_ms` |
| [`auxid.env`](src/ixx/auxid-env.ixx) | Environment variables, `executable_path`, standard directories |
| [`auxid.ipc`](src/ixx/auxid-ipc.ixx) | Pipes, `Channel`, `ChannelListener`, `InstanceLock` |
| [`auxid.proc`](src/ixx/auxid-proc.ixx) | `spawn`, `Process`, and `LifetimeGroup`, which ends child processes when it closes |
| [`auxid.dl`](src/ixx/auxid-dl.ixx) | `Library`: open, look up a symbol, close |
| [`auxid.test`](src/ixx/auxid-test.ixx) | The small test framework Auxid uses for its own tests |

Interfaces are in `src/ixx`, implementations in `src/cpp`, and the two public headers (`api.hpp` and `macros.hpp`) in `include/auxid`. rpmalloc and wyhash are vendored under `src/c/vendor` and `src/h/vendor`.

## Building and testing

```bash
cmake --preset x64-linux
cmake --build --preset x64-linux --config Release
ctest --preset x64-linux --output-on-failure
```

These are the main presets. [CMakePresets.json](CMakePresets.json) also has shared-library, sanitizer and benchmark ones.

| Target | Presets |
| --- | --- |
| Linux x64 | `x64-linux` (Clang), `x64-linux-gcc` |
| Linux ARM64 | `arm64-linux` (Clang cross), `arm64-linux-gcc` (GCC cross), `arm64-linux-native` |
| macOS | `arm64-darwin`, `x64-darwin` (Homebrew LLVM: `brew install llvm ninja`) |
| Windows x64 | `x64-windows-msvc`, `x64-windows-clang`, `x64-windows-mingw-gcc`, `x64-windows` (Visual Studio) |
| Windows ARM64 | `arm64-windows-msvc`, `arm64-windows-clang`, `arm64-windows` (Visual Studio) |
| WebAssembly | `wasm32-emscripten` (uses the system malloc instead of rpmalloc) |

[CI](.github/workflows/ci.yaml) runs on every push and pull request to `main`. It covers Linux x64, macOS ARM64, MSVC, Windows Clang (shared library), MinGW and WebAssembly, plus ASan, UBSan and TSan on Linux. The Linux, macOS and MSVC builds run at C++20 and C++23.

The library is static unless you pass `-DAuxid_BUILD_SHARED=ON`. CI tests the shared build on Windows only. On Linux the test suite doesn't link against it yet, and WebAssembly doesn't support it.

## Who wrote what

[AI-Human-Map.md](AI-Human-Map.md) records which parts of Auxid were written by people and which by AI, with the model named for each part. Run `python3 tools/authorship_map.py` when you change the code. CI checks it.

## Related

[Auxid-Project-Template](https://github.com/IASoft-PVT-LTD/Auxid-Project-Template) is a starting point for new projects, with CMake presets included.

## License

Copyright © 2026 IASoft (PVT) LTD. Licensed under the [Apache License, Version 2.0](LICENSE).

The vendored [rpmalloc](https://github.com/mjansson/rpmalloc) and [wyhash](https://github.com/wangyi-fudan/wyhash) keep their own licenses, recorded in their source headers.
