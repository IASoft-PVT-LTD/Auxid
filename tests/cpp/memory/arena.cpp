// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

import auxid;
import auxid.test;

using namespace au;

namespace
{
  struct ArenaBlock final : test::Block
  {
    [[nodiscard]] auto get_name() const -> const char * override
    {
      return "memory::arena";
    }

    auto declare_tests() -> void override
    {
      add_test("alloc", [this] { return alloc_(); });
      add_test("exhaustion", [this] { return exhaustion(); });
      add_test("clear", [this] { return clear_(); });
    }

    auto alloc_() -> bool
    {
      u8 buffer[1024];
      memory::ArenaAllocator arena;
      arena.init(buffer, sizeof(buffer));

      void *ptr1 = arena.alloc(16);
      if (!check_not(ptr1 == nullptr, "alloc(16) != nullptr"))
        return false;
      if (!check(arena.offset >= 16u, "arena.offset >= 16"))
        return false;

      void *ptr2 = arena.alloc(32);
      return check_not(ptr2 == nullptr, "alloc(32) != nullptr") && check(arena.offset >= 48u, "arena.offset >= 48");
    }

    auto exhaustion() -> bool
    {
      u8 buffer[64];
      memory::ArenaAllocator arena;
      arena.init(buffer, sizeof(buffer));

      void *ptr1 = arena.try_alloc(64);
      if (!check_not(ptr1 == nullptr, "try_alloc(64) succeeds"))
        return false;

      void *ptr2 = arena.try_alloc(8);
      return check_eq(ptr2, static_cast<void *>(nullptr), "try_alloc(8) returns nullptr after exhaustion");
    }

    auto clear_() -> bool
    {
      u8 buffer[128];
      memory::ArenaAllocator arena;
      arena.init(buffer, sizeof(buffer));

      (void) arena.alloc(64);
      if (!check(arena.offset >= 64u, "arena.offset >= 64 before clear"))
        return false;

      arena.clear();
      return check_eq(arena.offset, static_cast<usize>(0), "arena.offset == 0 after clear");
    }
  };

  const test::AutoRegister<ArenaBlock> _registered;
} // namespace
