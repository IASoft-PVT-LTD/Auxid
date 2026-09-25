// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

// OS-CONTRACT conformance: auxid.time.

import auxid;
import auxid.test;

using namespace au;

namespace
{
  struct TimeBlock final : test::Block
  {
    [[nodiscard]] auto get_name() const -> const char * override
    {
      return "os::time";
    }

    auto declare_tests() -> void override
    {
      add_test("monotonic_advances", [this] { return monotonic_advances(); });
      add_test("sleep_blocks_at_least", [this] { return sleep_blocks_at_least(); });
      add_test("wall_clock_sane", [this] { return wall_clock_sane(); });
    }

    auto monotonic_advances() -> bool
    {
      const u64 a = time::monotonic_ns();
      const u64 b = time::monotonic_ns();
      return check(b >= a, "monotonic never goes backwards");
    }

    auto sleep_blocks_at_least() -> bool
    {
      const u64 before = time::monotonic_ns();
      time::sleep_ms(30);
      const u64 elapsed_ms = (time::monotonic_ns() - before) / 1'000'000ull;
      // Contract: AT LEAST the requested duration (small tolerance for
      // coarse timers).
      return check(elapsed_ms >= 25, "sleep_ms(30) blocked for >= ~30ms");
    }

    auto wall_clock_sane() -> bool
    {
      // 2023-01-01 UTC in unix ms — a floor, not an exact expectation.
      return check(time::wall_unix_ms() > 1'672'531'200'000ll, "wall clock is after 2023");
    }
  };

  const test::AutoRegister<TimeBlock> _registered;
} // namespace
