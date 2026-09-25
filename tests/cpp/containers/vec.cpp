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
  struct VecBlock final : test::Block
  {
    [[nodiscard]] auto get_name() const -> const char * override
    {
      return "containers::vec";
    }

    auto declare_tests() -> void override
    {
      add_test("push_and_pop", [this] { return push_and_pop(); });
      add_test("reserve_and_capacity", [this] { return reserve_and_capacity(); });
      add_test("initializer_list", [this] { return initializer_list_(); });
      add_test("clear", [this] { return clear_(); });
    }

    auto push_and_pop() -> bool
    {
      Vec<i32> v;
      v.push_back(10);
      v.push_back(20);

      if (!check_eq(v.size(), 2u, "v.size() == 2"))
        return false;
      if (!check_eq(v[0], 10, "v[0] == 10"))
        return false;
      if (!check_eq(v[1], 20, "v[1] == 20"))
        return false;

      v.pop_back();
      return check_eq(v.size(), 1u, "v.size() == 1 after pop") && check_eq(v.back(), 10, "v.back() == 10 after pop");
    }

    auto reserve_and_capacity() -> bool
    {
      Vec<i32> v;
      v.reserve(100);
      return check(v.capacity() >= 100u, "v.capacity() >= 100") && check_eq(v.size(), 0u, "v.size() == 0") &&
             check(v.empty(), "v.empty() is true");
    }

    auto initializer_list_() -> bool
    {
      Vec<String> v = {"Rigid", "C++", "Auxid"};
      return check_eq(v.size(), 3u, "v.size() == 3") && check_eq(v[0], "Rigid", "v[0] == Rigid") &&
             check_eq(v[1], "C++", "v[1] == C++") && check_eq(v[2], "Auxid", "v[2] == Auxid");
    }

    auto clear_() -> bool
    {
      Vec<i32> v = {1, 2, 3, 4, 5};
      v.clear();
      return check(v.empty(), "v.empty() is true after clear") && check_eq(v.size(), 0u, "v.size() == 0 after clear") &&
             check(v.capacity() >= 5u, "v.capacity() >= 5 retained after clear");
    }
  };

  const test::AutoRegister<VecBlock> _registered;
} // namespace
