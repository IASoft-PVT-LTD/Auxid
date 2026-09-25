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
  struct HashMapBlock final : test::Block
  {
    [[nodiscard]] auto get_name() const -> const char * override
    {
      return "containers::hash_map";
    }

    auto declare_tests() -> void override
    {
      add_test("insert_and_find", [this] { return insert_and_find(); });
      add_test("erase", [this] { return erase_(); });
      add_test("operator_brackets", [this] { return operator_brackets(); });
      add_test("transparent_lookup", [this] { return transparent_lookup(); });
      add_test("default_seeds_differ_between_instances", [this] { return default_seeds_differ_between_instances(); });
      add_test("fixed_seed_constructor_is_reproducible", [this] { return fixed_seed_constructor_is_reproducible(); });
      add_test("high_h2_collision_stress", [this] { return high_h2_collision_stress(); });
      add_test("insert_erase_pingpong_compacts", [this] { return insert_erase_pingpong_compacts(); });
      add_test("iterate_after_many_erases", [this] { return iterate_after_many_erases(); });
      add_test("large_insert_and_lookup", [this] { return large_insert_and_lookup(); });
    }

    auto insert_and_find() -> bool
    {
      HashMap<i32, String> map;
      if (!check(map.insert(1, "One"), "insert(1, \"One\")"))
        return false;
      if (!check(map.insert(2, "Two"), "insert(2, \"Two\")"))
        return false;

      if (!check(map.contains(1), "map.contains(1)"))
        return false;
      if (!check_eq(*map.find(1), "One", "*map.find(1) == \"One\""))
        return false;

      if (!check_not(map.insert(1, "Duplicate"), "duplicate insert returns false"))
        return false;
      return check_eq(map.size(), 2u, "map.size() == 2");
    }

    auto erase_() -> bool
    {
      HashMap<i32, i32> map;
      map.insert(10, 100);
      map.insert(20, 200);

      if (!check(map.erase(10), "erase(10)"))
        return false;
      if (!check_not(map.contains(10), "!contains(10) after erase"))
        return false;
      if (!check_eq(map.size(), 1u, "size == 1 after erase"))
        return false;

      return check_not(map.erase(999), "erase(999) returns false");
    }

    auto operator_brackets() -> bool
    {
      HashMap<String, i32> map;
      map["Score"] = 150;
      if (!check_eq(map["Score"], 150, "map[\"Score\"] == 150"))
        return false;

      map["Score"] = 250;
      return check_eq(map["Score"], 250, "map[\"Score\"] == 250 after re-assign");
    }

    auto transparent_lookup() -> bool
    {
      HashMap<String, i32> map;
      map.insert(String("alpha"), 1);
      map.insert(String("beta"), 2);
      map.insert(String("gamma"), 3);

      if (!check(map.contains(StringView("alpha")), "contains(StringView)"))
        return false;
      if (!check(map.contains("beta"), "contains(const char*)"))
        return false;
      if (!check_not(map.contains(StringView("missing")), "!contains(missing)"))
        return false;

      i32 *p = map.find(StringView("gamma"));
      if (!check(p != nullptr, "find(StringView) hit"))
        return false;
      if (!check_eq(*p, 3, "*find(StringView) == 3"))
        return false;

      if (!check(map.erase("beta"), "erase(const char*)"))
        return false;
      if (!check_not(map.contains("beta"), "!contains(\"beta\") post-erase"))
        return false;
      return check_eq(map.size(), 2u, "size == 2 post-erase");
    }

    auto default_seeds_differ_between_instances() -> bool
    {
      u64 prev = HashMap<i32, i32>{}.seed();
      for (i32 i = 0; i < 8; ++i)
      {
        const u64 next = HashMap<i32, i32>{}.seed();
        if (!check_neq(prev, next, "consecutive HashMaps must draw fresh seeds"))
          return false;
        prev = next;
      }
      return true;
    }

    auto fixed_seed_constructor_is_reproducible() -> bool
    {
      constexpr u64 kSeed = 0xC0FFEE12345678AAULL;
      HashMap<i32, i32> a(0, kSeed);
      HashMap<i32, i32> b(0, kSeed);
      if (!check_eq(a.seed(), kSeed, "seed plumbed into HashMap"))
        return false;
      if (!check_eq(b.seed(), kSeed, "seed plumbed into HashMap (b)"))
        return false;
      for (i32 i = 0; i < 256; ++i)
      {
        a.insert(i, i * 2);
        b.insert(i, i * 2);
      }
      for (i32 i = 0; i < 256; ++i)
      {
        const i32 *va = a.find(i);
        const i32 *vb = b.find(i);
        if (!check(va != nullptr && vb != nullptr, "both maps find every key"))
          return false;
        if (!check_eq(*va, *vb, "values agree across same-seeded HashMaps"))
          return false;
      }
      return true;
    }

    auto high_h2_collision_stress() -> bool
    {
      constexpr i32 kN = 4096;
      HashMap<u64, i32> map;
      map.reserve(kN);
      for (i32 i = 0; i < kN; ++i)
      {
        const u64 key = static_cast<u64>(i) * 0x9E3779B97F4A7C15ULL;
        if (!check(map.insert(key, i), "insert distinct key"))
          return false;
      }
      if (!check_eq(map.size(), static_cast<usize>(kN), "all kN keys present"))
        return false;
      for (i32 i = 0; i < kN; ++i)
      {
        const u64 key = static_cast<u64>(i) * 0x9E3779B97F4A7C15ULL;
        const i32 *v = map.find(key);
        if (!check(v != nullptr, "stress key found"))
          return false;
        if (!check_eq(*v, i, "stress value matches"))
          return false;
      }
      return true;
    }

    auto insert_erase_pingpong_compacts() -> bool
    {
      HashMap<i32, i32> map(64, 0xA55A55A55A55A55AULL);
      for (i32 i = 0; i < 4096; ++i)
      {
        map.insert(i, i);
        if (i >= 64)
        {
          if (!check(map.erase(i - 64), "erase(i - 64) succeeds"))
            return false;
        }
      }
      if (!check_eq(map.size(), static_cast<usize>(64), "live set size held at 64"))
        return false;
      for (i32 i = 4096 - 64; i < 4096; ++i)
      {
        const i32 *v = map.find(i);
        if (!check(v != nullptr, "live key found"))
          return false;
        if (!check_eq(*v, i, "live value matches"))
          return false;
      }
      for (i32 i = 0; i < 4096 - 64; ++i)
      {
        if (!check_not(map.contains(i), "evicted key absent"))
          return false;
      }
      return true;
    }

    auto iterate_after_many_erases() -> bool
    {
      HashMap<i32, i32> map;
      for (i32 i = 0; i < 1024; ++i)
        map.insert(i, i * 3);
      for (i32 i = 0; i < 1024; i += 2)
        (void) map.erase(i);
      if (!check_eq(map.size(), static_cast<usize>(512), "size after half-erase"))
        return false;

      usize count = 0;
      u64 sum = 0;
      for (const auto &p : map)
      {
        ++count;
        sum += static_cast<u64>(p.second);
        if (!check(p.first % 2 == 1, "iterator only yields odd keys"))
          return false;
      }
      if (!check_eq(count, map.size(), "iteration visits every live entry"))
        return false;
      u64 expected = 0;
      for (i32 i = 1; i < 1024; i += 2)
        expected += static_cast<u64>(i * 3);
      return check_eq(sum, expected, "iteration sum matches expected odd sum");
    }

    auto large_insert_and_lookup() -> bool
    {
      constexpr i32 kN = 16384;
      HashMap<i32, i32> map;
      for (i32 i = 0; i < kN; ++i)
        map.insert(i, i ^ 0x55AA);
      if (!check_eq(map.size(), static_cast<usize>(kN), "all keys inserted"))
        return false;
      for (i32 i = 0; i < kN; ++i)
      {
        const i32 *v = map.find(i);
        if (!check(v != nullptr, "key found post-grow"))
          return false;
        if (!check_eq(*v, i ^ 0x55AA, "value matches post-grow"))
          return false;
      }
      return check_not(map.contains(kN + 1), "miss returns null");
    }
  };

  const test::AutoRegister<HashMapBlock> _registered;
} // namespace
