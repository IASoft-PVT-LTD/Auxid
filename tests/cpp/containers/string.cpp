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
  struct StringBlock final : test::Block
  {
    [[nodiscard]] auto get_name() const -> const char * override
    {
      return "containers::string";
    }

    auto declare_tests() -> void override
    {
      add_test("sso", [this] { return sso(); });
      add_test("heap_allocation", [this] { return heap_allocation(); });
      add_test("append_and_concat", [this] { return append_and_concat(); });
      add_test("push_pop", [this] { return push_pop(); });
      add_test("arena_basic_string", [this] { return arena_basic_string(); });
      add_test("find_needle_longer_than_haystack",
               [this] { return find_needle_longer_than_haystack(); });
    }

    auto find_needle_longer_than_haystack() -> bool
    {
      // Regression: h_len - n_len underflowed when the needle was longer than
      // the haystack, turning the search bound into ~SIZE_MAX (out-of-bounds
      // reads; found via a SEGV in LaVista's color-scheme query).
      const StringView haystack("hi");
      if (!check_eq(haystack.find(StringView("prefer-dark")), StringView::npos,
                    "needle longer than haystack is npos"))
        return false;
      if (!check_eq(StringView("").find(StringView("x")), StringView::npos,
                    "empty haystack is npos"))
        return false;
      if (!check_eq(haystack.find(StringView("hi")), 0u, "exact-length match still found"))
        return false;
      return check_eq(haystack.find(StringView("")), 0u, "empty needle matches at pos");
    }

    auto sso() -> bool
    {
      String s("Rigid");
      return check_eq(s.size(), 5u, "s.size() == 5") && check_eq(s, "Rigid", "s == \"Rigid\"");
    }

    auto heap_allocation() -> bool
    {
      String s("This string is deliberately long to bypass the SSO capacity.");
      return check(s.size() > 23u, "s.size() > 23 (heap path)") &&
             check_eq(s.substr(0, 4), "This", "s.substr(0,4) == \"This\"");
    }

    auto append_and_concat() -> bool
    {
      String s("Data");
      s += " Oriented";
      if (!check_eq(s, "Data Oriented", "after += \" Oriented\""))
        return false;

      String combined = s + String(" Design");
      return check_eq(combined, "Data Oriented Design", "concat result");
    }

    auto push_pop() -> bool
    {
      String s("C+");
      s.push_back('+');
      if (!check_eq(s, "C++", "after push_back('+')"))
        return false;
      s.pop_back();
      return check_eq(s, "C+", "after pop_back");
    }

    auto arena_basic_string() -> bool
    {
      using ArenaString = BasicString<memory::ArenaAllocator>;

      alignas(8) static u8 arena_buffer[1024];
      memory::ArenaAllocator arena;
      arena.init(arena_buffer, sizeof(arena_buffer));

      ArenaString sso(arena);
      sso.assign(StringView("Rigid"));
      if (!check_eq(sso.size(), 5u, "arena sso size"))
        return false;
      if (!check_eq(sso, StringView("Rigid"), "arena sso content"))
        return false;

      ArenaString heap(arena);
      heap.reserve(128);
      heap.assign(StringView("This string is deliberately long to bypass SSO capacity."));
      if (!check(heap.size() > 23u, "arena heap path size > 23"))
        return false;
      if (!check_eq(heap.substr(0, 4), StringView("This"), "arena heap substr(0,4)"))
        return false;

      heap.append(StringView(" tail"));
      return check(heap.size() > 50u, "arena heap appended");
    }
  };

  const test::AutoRegister<StringBlock> _registered;
} // namespace
