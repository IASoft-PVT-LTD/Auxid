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

#include <chrono>
#include <thread>

module auxid.time;

namespace au::time
{
  AUXID_API auto monotonic_ns() -> u64
  {
    return static_cast<u64>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
  }

  AUXID_API auto wall_unix_ms() -> i64
  {
    return static_cast<i64>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count());
  }

  AUXID_API auto sleep_ms(u32 milliseconds) -> void
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
  }
} // namespace au::time
