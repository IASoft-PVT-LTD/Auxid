# AI-Human Map

This file records which parts of Auxid were written by people and which by AI, and names the AI model for each AI-written part. It is measured from git history as of commit `e35f587` (2026-11-24). This file was written by Claude Opus 5.5 and is not counted in the numbers below.

## Key stats

| | Human | Claude Fable 5 | Claude Opus 5.5 | All AI |
| --- | --- | --- | --- | --- |
| Whole repository, 15,079 lines | **68.7%** | 22.4% | 8.8% | **31.3%** |
| Library only, 8,537 lines | **62.4%** | 30.8% | 6.8% | **37.6%** |
| Library without the OS layer, 6,322 lines | **84.1%** | 6.8% | 9.1% | **15.9%** |
| Commits, 139 | 113, including 4 merges | 19 | 7 | 26 |

Every commit marked as AI is from 2026-08-26 or later. Percentages are rounded and may not add up to exactly 100.

## By area

| Area | Lines | Human | Claude Fable 5 | Claude Opus 5.5 | Files with no AI code commits |
| --- | --- | --- | --- | --- | --- |
| Library, without the OS layer | 6,322 | 5,319 (84.1%) | 429 (6.8%) | 574 (9.1%) | 1 of 13 |
| OS layer | 2,215 | 8 (0.4%) | 2,202 (99.4%) | 5 (0.2%) | 0 of 11 |
| Tests | 2,606 | 1,377 (52.8%) | 602 (23.1%) | 627 (24.1%) | 11 of 23 |
| Benchmarks | 1,680 | 1,680 (100.0%) | 0 (0.0%) | 0 (0.0%) | 10 of 11 |
| Build files and presets | 1,358 | 1,325 (97.6%) | 27 (2.0%) | 6 (0.4%) | 19 of 23 |
| CI and config | 639 | 623 (97.5%) | 0 (0.0%) | 16 (2.5%) | 7 of 8 |
| README and docs | 259 | 34 (13.1%) | 124 (47.9%) | 101 (39.0%) | 0 of 2 |
| **Total** | 15,079 | 10,366 (68.7%) | 3,384 (22.4%) | 1,329 (8.8%) | 48 of 91 |

The OS layer is `auxid.time`, `auxid.env`, `auxid.ipc`, `auxid.proc`, `auxid.dl` and `auxid_win32.hpp`. Claude Fable 5 wrote it on 2026-08-26, apart from five lines Claude Opus 5.5 added later.

## Files with AI-written lines

Each count is the number of lines that survive today. The few human lines in files an AI created are boilerplate copied from existing files.

### Library, without the OS layer

| File | Lines | Human | Claude Fable 5 | Claude Opus 5.5 | What the AI wrote |
| --- | --- | --- | --- | --- | --- |
| [include/auxid/macros.hpp](include/auxid/macros.hpp) | 110 | 93 (85%) | 11 | 6 | Fable 5: function attributes. Opus 5.5: C++20 check, `<utility>` include |
| [src/cpp/auxid-containers.cpp](src/cpp/auxid-containers.cpp) | 39 | 25 (64%) | 14 | 0 | Fable 5: `Error::os_last` |
| [src/cpp/auxid-core.cpp](src/cpp/auxid-core.cpp) | 109 | 24 (22%) | 0 | 85 | Opus 5.5: console output, panic printing |
| [src/cpp/auxid-test.cpp](src/cpp/auxid-test.cpp) | 68 | 54 (79%) | 0 | 14 | Opus 5.5: printing |
| [src/cpp/auxid-thread.cpp](src/cpp/auxid-thread.cpp) | 281 | 121 (43%) | 142 | 18 | Fable 5: the native thread backend. Opus 5.5: log line output |
| [src/ixx/auxid-containers.ixx](src/ixx/auxid-containers.ixx) | 3,265 | 2,966 (91%) | 153 | 146 | Fable 5: nearly all of `Error`, parts of `BasicString`. Opus 5.5: allocator handling in `BasicString`, `CompactVecBase`, `SlotMap` and the hash containers |
| [src/ixx/auxid-core.ixx](src/ixx/auxid-core.ixx) | 557 | 512 (92%) | 29 | 16 | Fable 5: `ResultT`. Opus 5.5: `std::expected` guards, console output declarations |
| [src/ixx/auxid-fs.ixx](src/ixx/auxid-fs.ixx) | 450 | 446 (99%) | 4 | 0 | Fable 5: small edits |
| [src/ixx/auxid-memory.ixx](src/ixx/auxid-memory.ixx) | 861 | 565 (66%) | 14 | 282 | Opus 5.5: `AllocatorRef`, most of `ArenaAllocator`, the allocator handle concepts, assignment in `Arc` and `IntrusiveArc`. Fable 5: small edits |
| [src/ixx/auxid-test.ixx](src/ixx/auxid-test.ixx) | 313 | 306 (98%) | 0 | 7 | Opus 5.5: printing |
| [src/ixx/auxid-thread.ixx](src/ixx/auxid-thread.ixx) | 242 | 185 (76%) | 57 | 0 | Fable 5: `ThreadT`, `NativeThread` |
| [src/ixx/auxid.ixx](src/ixx/auxid.ixx) | 11 | 6 (55%) | 5 | 0 | Fable 5: the OS layer exports |

### OS layer

| File | Lines | Human | Claude Fable 5 | Claude Opus 5.5 |
| --- | --- | --- | --- | --- |
| [src/cpp/auxid-dl.cpp](src/cpp/auxid-dl.cpp) | 102 | 2 (2%) | 100 | 0 |
| [src/cpp/auxid-env.cpp](src/cpp/auxid-env.cpp) | 330 | 0 (0%) | 330 | 0 |
| [src/cpp/auxid-ipc.cpp](src/cpp/auxid-ipc.cpp) | 542 | 2 (0%) | 540 | 0 |
| [src/cpp/auxid-proc.cpp](src/cpp/auxid-proc.cpp) | 529 | 2 (0%) | 527 | 0 |
| [src/cpp/auxid-time.cpp](src/cpp/auxid-time.cpp) | 24 | 0 (0%) | 24 | 0 |
| [src/hpp/auxid_win32.hpp](src/hpp/auxid_win32.hpp) | 166 | 0 (0%) | 161 | 5 |
| [src/ixx/auxid-dl.ixx](src/ixx/auxid-dl.ixx) | 56 | 0 (0%) | 56 | 0 |
| [src/ixx/auxid-env.ixx](src/ixx/auxid-env.ixx) | 41 | 0 (0%) | 41 | 0 |
| [src/ixx/auxid-ipc.ixx](src/ixx/auxid-ipc.ixx) | 248 | 2 (1%) | 246 | 0 |
| [src/ixx/auxid-proc.ixx](src/ixx/auxid-proc.ixx) | 153 | 0 (0%) | 153 | 0 |
| [src/ixx/auxid-time.ixx](src/ixx/auxid-time.ixx) | 24 | 0 (0%) | 24 | 0 |

### Tests

| File | Lines | Human | Claude Fable 5 | Claude Opus 5.5 |
| --- | --- | --- | --- | --- |
| [tests/CMakeLists.txt](tests/CMakeLists.txt) | 58 | 41 (71%) | 16 | 1 |
| [tests/cpp/containers/string.cpp](tests/cpp/containers/string.cpp) | 149 | 74 (50%) | 18 | 57 |
| [tests/cpp/core/result.cpp](tests/cpp/core/result.cpp) | 178 | 82 (46%) | 88 | 8 |
| [tests/cpp/main.cpp](tests/cpp/main.cpp) | 24 | 21 (88%) | 0 | 3 |
| [tests/cpp/memory/arc.cpp](tests/cpp/memory/arc.cpp) | 86 | 8 (9%) | 0 | 78 |
| [tests/cpp/memory/arena.cpp](tests/cpp/memory/arena.cpp) | 563 | 83 (15%) | 0 | 480 |
| [tests/cpp/memory/foreign_thread.cpp](tests/cpp/memory/foreign_thread.cpp) | 49 | 8 (16%) | 41 | 0 |
| [tests/cpp/os/dl.cpp](tests/cpp/os/dl.cpp) | 74 | 8 (11%) | 66 | 0 |
| [tests/cpp/os/env.cpp](tests/cpp/os/env.cpp) | 91 | 8 (9%) | 83 | 0 |
| [tests/cpp/os/ipc.cpp](tests/cpp/os/ipc.cpp) | 117 | 8 (7%) | 109 | 0 |
| [tests/cpp/os/proc.cpp](tests/cpp/os/proc.cpp) | 156 | 8 (5%) | 148 | 0 |
| [tests/cpp/os/time.cpp](tests/cpp/os/time.cpp) | 41 | 8 (20%) | 33 | 0 |

### Build files and presets

| File | Lines | Human | Claude Fable 5 | Claude Opus 5.5 |
| --- | --- | --- | --- | --- |
| [CMakeLists.txt](CMakeLists.txt) | 49 | 48 (98%) | 1 | 0 |
| [cmake/auxid_setup_project.cmake](cmake/auxid_setup_project.cmake) | 66 | 60 (91%) | 0 | 6 |
| [cmake/toolchains/x64-windows-mingw-gcc.cmake](cmake/toolchains/x64-windows-mingw-gcc.cmake) | 17 | 4 (24%) | 13 | 0 |
| [src/CMakeLists.txt](src/CMakeLists.txt) | 119 | 106 (89%) | 13 | 0 |

### CI and config

| File | Lines | Human | Claude Fable 5 | Claude Opus 5.5 |
| --- | --- | --- | --- | --- |
| [.github/workflows/ci.yaml](.github/workflows/ci.yaml) | 236 | 220 (93%) | 0 | 16 |

### README and docs

| File | Lines | Human | Claude Fable 5 | Claude Opus 5.5 |
| --- | --- | --- | --- | --- |
| [README.md](README.md) | 136 | 34 (25%) | 1 | 101 |
| [docs/OS-CONTRACT.md](docs/OS-CONTRACT.md) | 123 | 0 (0%) | 123 | 0 |

`benchmarks/CMakeLists.txt` has no AI-written lines, but Claude Opus 5.5 deleted three lines from it.

## Files with no AI code commits

The two AI relicense commits changed the licence header in most of these. Nothing else in them has an AI commit.

- **Library, without the OS layer:** `include/auxid/api.hpp`
- **Tests:** `tests/cpp/containers/format.cpp`, `tests/cpp/containers/hash_base.cpp`, `tests/cpp/containers/hash_map.cpp`, `tests/cpp/containers/hash_set.cpp`, `tests/cpp/containers/iterator_concepts.cpp`, `tests/cpp/containers/option.cpp`, `tests/cpp/containers/pair.cpp`, `tests/cpp/containers/slot_map.cpp`, `tests/cpp/containers/vec.cpp`, `tests/cpp/memory/heap.cpp`, `tests/cpp/thread/thread.cpp`
- **Benchmarks:** `benchmarks/build.py`, `benchmarks/cpp/containers/hash_map_shared.hpp`, `benchmarks/cpp/containers/hash_map_vs_unordered_map.cpp`, `benchmarks/cpp/core/exception_baseline.cpp`, `benchmarks/cpp/core/exception_baseline_shared.hpp`, `benchmarks/cpp/core/result_vs_exceptions.cpp`, `benchmarks/cpp/core/result_vs_exceptions_shared.hpp`, `benchmarks/cpp/main.cpp`, `benchmarks/generate_csv.py`, `benchmarks/run.py`
- **Build files and presets:** `CMakePresets.json`, `cmake/AuxidConfig.cmake.in`, `cmake/auxid_fetch_google_benchmark.cmake`, `cmake/auxid_install.cmake`, `cmake/auxid_lint_ban_list.cmake`, `cmake/auxid_msvc_pgo.cmake`, `cmake/auxid_sanitizers.cmake`, `cmake/toolchains/arm64-darwin-clang.cmake`, `cmake/toolchains/arm64-linux-clang.cmake`, `cmake/toolchains/arm64-linux-gcc.cmake`, `cmake/toolchains/arm64-linux-native-clang.cmake`, `cmake/toolchains/arm64-windows-clang.cmake`, `cmake/toolchains/arm64-windows-msvc.cmake`, `cmake/toolchains/wasm32-emscripten-clang.cmake`, `cmake/toolchains/x64-darwin-clang.cmake`, `cmake/toolchains/x64-linux-clang.cmake`, `cmake/toolchains/x64-linux-gcc.cmake`, `cmake/toolchains/x64-windows-clang.cmake`, `cmake/toolchains/x64-windows-msvc.cmake`
- **CI and config:** `.clang-format`, `.clang-tidy`, `.github/workflows/package.yaml`, `.gitignore`, `.vscode/launch.json`, `.vscode/settings.json`, `.vscode/tasks.json`

In files that do have AI commits, these types have no AI-written lines: `StringView`, `Hash`, `EqualTo`, `Group`, `BitMask`, `ProbeSeq`, `RefCounted`, `Mutex`, `LockGuard`, `ConditionVariable`, `MainThreadGuard`, the `Logger` declaration, and the test `Runner`, `TestRegistry` and `AutoRegister`.

## AI commits

| Commit | Date | Model | Subject |
| --- | --- | --- | --- |
| `e35f587` | 2026-11-24 | Claude Opus 5.5 | [MAINTAIN]: Rewrite README |
| `9591d52` | 2026-11-24 | Claude Opus 5.5 | [DESIGN]: Lower The Minimum Standard From C++23 To C++20 |
| `95fc3ab` | 2026-11-24 | Claude Opus 5.5 | [FIX]: CI - Pin Linux Jobs To Ubuntu 24.04, Upgrade LLVM On The Windows Clang Job |
| `9b0b477` | 2026-11-24 | Claude Opus 5.5 | [FIX]: Reuse buffers on same-arena copies, make debug handle counts thread-safe, reject unshared allocators in make_* |
| `f05575f` | 2026-10-04 | Claude Opus 5.5 | [FIX]: Take allocator handles in every container, check arena resets in debug, implement arena realloc |
| `fe3908a` | 2026-10-04 | Claude Opus 5.5 | [FIX]: Share stateful allocators through AllocatorRef instead of copying them |
| `1a658b5` | 2026-09-25 | Claude Opus 5.5 | [MAINTAIN]: Relicense under Apache License 2.0 |
| `c71bd32` | 2026-08-26 | Claude Fable 5 | [FIX]: StringView::find out-of-bounds read when the needle exceeds the haystack |
| `83ec221` | 2026-08-26 | Claude Fable 5 | [FIX]: Make String/StringView formatters context-generic; drop zero-arg println |
| `111dca5` | 2026-08-26 | Claude Fable 5 | [DESIGN]: OS Foundation Layer Slice 3 - auxid.proc + auxid.dl (D-010 Complete) |
| `96b5041` | 2026-08-26 | Claude Fable 5 | [DESIGN]: OS Foundation Layer Slice 2 - auxid.ipc (D-010) |
| `736bd1c` | 2026-08-26 | Claude Fable 5 | [MAINTAIN]: Remove FUNDING.yml |
| `ef1f3e0` | 2026-08-26 | Claude Fable 5 | [LEGAL]: Relicense To PolyForm Noncommercial 1.0.0 |
| `db17074` | 2026-08-26 | Claude Fable 5 | [FIX]: macOS ICE Workaround Round 2 - Minimize Path Codegen In auxid-env.cpp |
| `221177f` | 2026-08-26 | Claude Fable 5 | [FIX]: WASM Honest Absence + macOS ICE Workaround For auxid.env |
| `9b20b83` | 2026-08-26 | Claude Fable 5 | [FIX]: MinGW Runtime Linking Policy - Dynamic libgcc/winpthread, Pinned DLLs |
| `31d6e7b` | 2026-08-26 | Claude Fable 5 | [DESIGN]: OS Foundation Layer Slice 1 - auxid.time + auxid.env (D-010) |
| `b3eada5` | 2026-08-26 | Claude Fable 5 | [MAINTAIN]: IASoft-PVT-LTD Org Move + Version 0.7.0 |
| `14f1115` | 2026-08-26 | Claude Fable 5 | [DESIGN]: Structured Error Type - Result&lt;T&gt; Now Defaults To au::Error (D-008) |
| `296e343` | 2026-08-26 | Claude Fable 5 | [FIX]: MinGW Modules Build - Drop windows.h From Thread Implementation Unit |
| `73e5453` | 2026-08-26 | Claude Fable 5 | [MAINTAIN]: README Corrections |
| `abe6241` | 2026-08-26 | Claude Fable 5 | [FIX]: String::vformat Formats Directly Into BasicString |
| `c89e5a6` | 2026-08-26 | Claude Fable 5 | [DESIGN]: Rename pure_fn/const_fn To stateless_fn/readonly_fn |
| `7d69545` | 2026-08-26 | Claude Fable 5 | [FIX]: Document Foreign-Thread Allocation Guarantee; Allocator Overflow Guard |
| `ee9fe3e` | 2026-08-26 | Claude Fable 5 | [DESIGN]: Native Thread Backend With Real Spawn Result; Registry Thread-Safety |
| `a78a06c` | 2026-08-26 | Claude Fable 5 | [FIX]: Result Errors Now Move Through AU_TRY; ResultT&lt;void&gt; Copy Symmetry |

The 19 Claude Fable 5 commits and the first three Claude Opus 5.5 commits carry a human author name with a `Co-Authored-By` trailer. Later ones have Claude as the author.

## How this is measured

- A commit counts as AI when its author is Claude or it has a `Co-Authored-By` trailer that names an AI model. The model name comes from the trailer.
- Each line goes to the commit that last changed it: `git blame -w -M -C -C`, which ignores whitespace and follows moved and copied code back to its source. Without following moves and copies, AI rises from 31.3% to 32.3% of the repository.
- Blank lines and licence headers are left out, and so are `LICENSE`, the logo, vendored code and benchmark results.

What it cannot show:

- A commit without a marker counts as human. Git cannot show AI help that left no marker.
- `bd49513` brought the module rewrite in from another repository as one commit. Its earlier history is not here, so it counts as human.
- It counts lines that survive. Human lines that AI later replaced are not shown, and an AI commit may include human edits.

## Keeping it current

- Every commit with AI-written code names the model in a trailer, for example `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- A pull request that changes code updates this file: the commit it is measured at, the numbers, and the AI commit list.
