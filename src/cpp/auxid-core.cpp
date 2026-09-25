// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

module;

#include <auxid/macros.hpp>

#include <cstdio>
#include <cstdlib>
#include <print>
#include <source_location>

module auxid.core;

namespace au
{
#if !defined(AUXID_DISABLE_DEFAULT_PANIC_HANDLER)
  [[noreturn]] AUXID_API auto panic_handler(const char *msg, const char *file, u32 line) -> void
  {
    std::println(stderr, "[PANIC]: ({}:{}): {}", file, line, msg);
    std::abort();
  }
#endif

  [[noreturn]] AUXID_API auto panic_at(const char *msg, const char *file, u32 line) -> void
  {
    panic_handler(msg, file, line);
    compiler::trap();
  }

  [[noreturn]] AUXID_API auto panic(const char *msg, std::source_location loc) -> void
  {
    panic_at(msg, loc.file_name(), loc.line());
  }
} // namespace au
