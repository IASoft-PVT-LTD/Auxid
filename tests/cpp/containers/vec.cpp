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
      add_test("compact_push_own_element", [this] { return compact_push_own_element(); });
      add_test("compact_resize_own_element", [this] { return compact_resize_own_element(); });
      add_test("tiny_up_to_the_limit", [this] { return tiny_up_to_the_limit(); });
    }

    auto compact_push_own_element() -> bool
    {
      const StringView text("long enough to be on the heap, 0123456789");
      CompactVec<String> strings;
      CompactVec<i32> numbers;
      strings.push_back(String(text));
      numbers.push_back(77);
      for (i32 i = 0; i < 100; i++)
      {
        strings.push_back(strings[0]);
        numbers.emplace_back(numbers[0]);
      }
      for (u32 i = 0; i < strings.size(); i++)
        if (!check_eq(strings[i], text, "copy of the first string") ||
            !check_eq(numbers[i], 77, "copy of the first number"))
          return false;

      while (strings.size() < strings.capacity())
        strings.push_back(strings[0]);
      strings.push_back(std::move(strings[1]));
      return check_eq(strings.back(), text, "own element moved to the end") &&
             check(strings[1].empty(), "and left empty");
    }

    auto compact_resize_own_element() -> bool
    {
      const StringView text("long enough to be on the heap, 0123456789");
      CompactVec<String> v;
      v.push_back(String(text));
      v.resize(50, v[0]);
      return check_eq(v.size(), 50u, "resized") && check_eq(v[49], text, "filled with its own element");
    }

    auto tiny_up_to_the_limit() -> bool
    {
      TinyVec<u8> v;
      for (u32 i = 0; i < 65535; i++)
        v.push_back(static_cast<u8>(i));
      return check_eq(static_cast<u32>(v.size()), 65535u, "65,535 elements fit") &&
             check_eq(v.back(), static_cast<u8>(254), "last one");
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
