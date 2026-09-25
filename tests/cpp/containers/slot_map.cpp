// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

#include <iterator>
#include <ranges>

import auxid;
import auxid.test;

using namespace au;

static_assert(std::contiguous_iterator<SlotMap<i32>::iterator>);
static_assert(std::contiguous_iterator<SlotMap<i32>::const_iterator>);
static_assert(std::ranges::contiguous_range<SlotMap<i32>>);

namespace
{
  struct SlotMapBlock final : test::Block
  {
    [[nodiscard]] auto get_name() const -> const char * override
    {
      return "containers::slot_map";
    }

    auto declare_tests() -> void override
    {
      add_test("insert_and_get", [this] { return insert_and_get(); });
      add_test("erase_invalidates_key", [this] { return erase_invalidates_key(); });
      add_test("generation_distinguishes_reused_slot", [this] { return generation_distinguishes_reused_slot(); });
      add_test("dense_iteration", [this] { return dense_iteration(); });
      add_test("contains_and_size", [this] { return contains_and_size(); });
      add_test("clear_resets_all", [this] { return clear_resets_all(); });
    }

    auto insert_and_get() -> bool
    {
      SlotMap<i32> sm;
      auto k1 = sm.insert(10);
      auto k2 = sm.insert(20);
      auto k3 = sm.insert(30);

      if (!check_eq(sm.size(), 3u, "size after 3 inserts"))
        return false;
      if (!check_eq(*sm.get(k1), 10, "get(k1) == 10"))
        return false;
      if (!check_eq(*sm.get(k2), 20, "get(k2) == 20"))
        return false;
      return check_eq(*sm.get(k3), 30, "get(k3) == 30");
    }

    auto erase_invalidates_key() -> bool
    {
      SlotMap<i32> sm;
      auto k1 = sm.insert(100);
      auto k2 = sm.insert(200);

      if (!check(sm.erase(k1), "erase(k1) returns true"))
        return false;
      if (!check(sm.get(k1) == nullptr, "get(k1) returns nullptr after erase"))
        return false;
      if (!check_not(sm.erase(k1), "double-erase returns false"))
        return false;
      return check_eq(*sm.get(k2), 200, "k2 survives erase of k1");
    }

    auto generation_distinguishes_reused_slot() -> bool
    {
      SlotMap<i32> sm;
      auto k1 = sm.insert(7);
      const u32 reused_idx = k1.idx;

      if (!check(sm.erase(k1), "first erase succeeds"))
        return false;

      auto k2 = sm.insert(42);
      if (!check_eq(k2.idx, reused_idx, "slot index reused"))
        return false;
      if (!check(k2.gen != k1.gen, "generation differs after reuse"))
        return false;
      if (!check(sm.get(k1) == nullptr, "stale key still returns nullptr"))
        return false;
      return check_eq(*sm.get(k2), 42, "fresh key returns new value");
    }

    auto dense_iteration() -> bool
    {
      SlotMap<i32> sm;
      (void) sm.insert(1);
      auto k_mid = sm.insert(2);
      (void) sm.insert(3);
      (void) sm.insert(4);

      sm.erase(k_mid);

      i32 sum = 0;
      usize count = 0;
      for (i32 v : sm)
      {
        sum += v;
        ++count;
      }

      if (!check_eq(count, sm.size(), "iteration count == size"))
        return false;
      return check_eq(sum, 1 + 3 + 4, "iteration sum after middle erase");
    }

    auto contains_and_size() -> bool
    {
      SlotMap<i32> sm;
      auto k = sm.insert(99);

      if (!check(sm.contains(k), "contains(k) is true"))
        return false;
      if (!check_eq(sm.size(), 1u, "size == 1"))
        return false;

      sm.erase(k);
      if (!check_not(sm.contains(k), "contains(k) is false after erase"))
        return false;
      return check(sm.empty(), "empty() after erase");
    }

    auto clear_resets_all() -> bool
    {
      SlotMap<i32> sm;
      auto k1 = sm.insert(1);
      (void) sm.insert(2);
      (void) sm.insert(3);

      sm.clear();

      if (!check(sm.empty(), "empty after clear"))
        return false;
      if (!check_eq(sm.size(), 0u, "size == 0 after clear"))
        return false;
      return check(sm.get(k1) == nullptr, "old keys invalid after clear");
    }
  };

  const test::AutoRegister<SlotMapBlock> _registered;
} // namespace
