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
      add_test("arena_string_grows", [this] { return arena_string_grows(); });
      add_test("arena_string_copy_assign", [this] { return arena_string_copy_assign(); });
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
      using ArenaRef = memory::AllocatorRef<memory::ArenaAllocator>;
      using ArenaString = BasicString<ArenaRef>;

      alignas(8) static u8 arena_buffer[1024];
      memory::ArenaAllocator arena;
      arena.init(arena_buffer, sizeof(arena_buffer));
      const ArenaRef arena_ref(arena);

      ArenaString sso(arena_ref);
      sso.assign(StringView("Rigid"));
      if (!check_eq(sso.size(), 5u, "arena sso size"))
        return false;
      if (!check_eq(sso, StringView("Rigid"), "arena sso content"))
        return false;

      ArenaString heap(arena_ref);
      heap.reserve(128);
      if (!check(arena.offset >= 129u, "arena offset advanced by heap reserve"))
        return false;
      heap.assign(StringView("This string is deliberately long to bypass SSO capacity."));
      if (!check(heap.size() > 23u, "arena heap path size > 23"))
        return false;
      if (!check_eq(heap.substr(0, 4), StringView("This"), "arena heap substr(0,4)"))
        return false;

      heap.append(StringView(" tail"));
      return check(heap.size() > 50u, "arena heap appended");
    }

    auto arena_string_grows() -> bool
    {
      using ArenaRef = memory::AllocatorRef<memory::ArenaAllocator>;
      using ArenaString = BasicString<ArenaRef>;

      alignas(8) static u8 arena_buffer[4096];
      memory::ArenaAllocator arena;
      arena.init(arena_buffer, sizeof(arena_buffer));
      const ArenaRef arena_ref(arena);

      ArenaString s(arena_ref);
      for (usize i = 0; i < 40; ++i)
        s.push_back(static_cast<char>('a' + (i % 26)));
      if (!check_eq(s.size(), 40u, "grew past the inline buffer"))
        return false;

      // Growing again goes through ArenaAllocator::realloc.
      for (usize i = 40; i < 300; ++i)
        s.push_back(static_cast<char>('a' + (i % 26)));
      if (!check_eq(s.size(), 300u, "grew a second time"))
        return false;

      const auto *p = reinterpret_cast<const u8 *>(s.data());
      if (!check(p >= arena_buffer && p < arena_buffer + sizeof(arena_buffer), "storage lives in the arena"))
        return false;
      for (usize i = 0; i < 300; ++i)
      {
        if (s.data()[i] != static_cast<char>('a' + (i % 26)))
          return check(false, "contents survive both growths");
      }
      return true;
    }

    auto arena_string_copy_assign() -> bool
    {
      using ArenaRef = memory::AllocatorRef<memory::ArenaAllocator>;
      using ArenaString = BasicString<ArenaRef>;

      alignas(8) static u8 buffer_a[512];
      alignas(8) static u8 buffer_b[512];
      memory::ArenaAllocator arena_a;
      memory::ArenaAllocator arena_b;
      arena_a.init(buffer_a, sizeof(buffer_a));
      arena_b.init(buffer_b, sizeof(buffer_b));
      const ArenaRef ref_a(arena_a);
      const ArenaRef ref_b(arena_b);

      ArenaString dst(StringView("destination string, long enough for the heap"), ref_a);
      const ArenaString src(StringView("source string, also long enough for the heap path"), ref_b);
      dst = src;

      if (!check(dst.get_allocator() == ref_b, "copy assignment takes the source's allocator"))
        return false;
      const auto *p = reinterpret_cast<const u8 *>(dst.data());
      if (!check(p >= buffer_b && p < buffer_b + sizeof(buffer_b), "copied storage comes from the source's arena"))
        return false;

      const ArenaString copy(src);
      return check(copy.get_allocator() == ref_b, "copy construction takes the source's allocator") &&
             check_eq(dst, src, "contents copied");
    }
  };

  const test::AutoRegister<StringBlock> _registered;
} // namespace
