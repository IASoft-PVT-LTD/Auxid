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
  struct HashSetBlock final : test::Block
  {
    [[nodiscard]] auto get_name() const -> const char * override
    {
      return "containers::hash_set";
    }

    auto declare_tests() -> void override
    {
      add_test("insert_and_contains", [this] { return insert_and_contains(); });
      add_test("erase_and_clear", [this] { return erase_and_clear(); });
      add_test("high_h2_collision_stress", [this] { return high_h2_collision_stress(); });
      add_test("insert_erase_pingpong_compacts", [this] { return insert_erase_pingpong_compacts(); });
      add_test("default_seeds_differ_between_instances", [this] { return default_seeds_differ_between_instances(); });
    }

    auto insert_and_contains() -> bool
    {
      HashSet<String> set;

      if (!check(set.insert("Core"), "insert(\"Core\")"))
        return false;
      if (!check(set.insert("Renderer"), "insert(\"Renderer\")"))
        return false;

      if (!check(set.contains("Core"), "contains(\"Core\")"))
        return false;
      if (!check(set.contains("Renderer"), "contains(\"Renderer\")"))
        return false;
      if (!check_not(set.contains("Physics"), "!contains(\"Physics\")"))
        return false;

      if (!check_not(set.insert("Core"), "duplicate insert returns false"))
        return false;
      return check_eq(set.size(), 2u, "set.size() == 2");
    }

    auto erase_and_clear() -> bool
    {
      HashSet<i32> set;
      set.insert(10);
      set.insert(20);
      set.insert(30);

      if (!check(set.erase(20), "erase(20)"))
        return false;
      if (!check_not(set.contains(20), "!contains(20) after erase"))
        return false;
      if (!check_eq(set.size(), 2u, "size == 2 after erase"))
        return false;
      if (!check_not(set.erase(999), "erase(999) returns false"))
        return false;

      set.clear();
      return check(set.empty(), "set.empty() after clear") && check_eq(set.size(), 0u, "size == 0 after clear");
    }

    auto high_h2_collision_stress() -> bool
    {
      constexpr i32 kN = 4096;
      HashSet<u64> set;
      set.reserve(kN);
      for (i32 i = 0; i < kN; ++i)
      {
        const u64 key = static_cast<u64>(i) * 0x9E3779B97F4A7C15ULL;
        if (!check(set.insert(key), "insert distinct key"))
          return false;
      }
      if (!check_eq(set.size(), static_cast<usize>(kN), "all keys present"))
        return false;
      for (i32 i = 0; i < kN; ++i)
      {
        const u64 key = static_cast<u64>(i) * 0x9E3779B97F4A7C15ULL;
        if (!check(set.contains(key), "stress key found"))
          return false;
      }
      return true;
    }

    auto insert_erase_pingpong_compacts() -> bool
    {
      HashSet<i32> set(64, 0xA55A55A55A55A55AULL);
      for (i32 i = 0; i < 4096; ++i)
      {
        set.insert(i);
        if (i >= 64)
        {
          if (!check(set.erase(i - 64), "erase(i - 64)"))
            return false;
        }
      }
      if (!check_eq(set.size(), static_cast<usize>(64), "live set size held at 64"))
        return false;
      for (i32 i = 4096 - 64; i < 4096; ++i)
      {
        if (!check(set.contains(i), "live key still in set"))
          return false;
      }
      return true;
    }

    auto default_seeds_differ_between_instances() -> bool
    {
      u64 prev = HashSet<i32>{}.seed();
      for (i32 i = 0; i < 8; ++i)
      {
        const u64 next = HashSet<i32>{}.seed();
        if (!check_neq(prev, next, "consecutive HashSets must draw fresh seeds"))
          return false;
        prev = next;
      }
      return true;
    }
  };

  const test::AutoRegister<HashSetBlock> _registered;
} // namespace
