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
  using ArenaRef = memory::AllocatorRef<memory::ArenaAllocator>;
  using ArenaAdapter = memory::StdAllocatorAdapter<i32, ArenaRef>;

  template<typename A>
  concept AdaptableAllocator = requires { typename memory::StdAllocatorAdapter<i32, A>; };

  template<typename A>
  concept VecAllocator = requires { typename Vec<i32, A>; };

  template<typename A>
  concept CopyableAllocator = requires(const A &a) { A{a}; };

  // An arena held by value would give every container copy its own bump offset
  // (two vectors receiving the same memory), so that must not compile.
  static_assert(memory::AllocatorType<memory::ArenaAllocator>);
  static_assert(!CopyableAllocator<memory::ArenaAllocator>);
  static_assert(!AdaptableAllocator<memory::ArenaAllocator>);
  static_assert(!VecAllocator<memory::ArenaAllocator>);

  static_assert(memory::AllocatorType<ArenaRef>);
  static_assert(AdaptableAllocator<ArenaRef>);
  static_assert(VecAllocator<ArenaRef>);
  static_assert(AdaptableAllocator<memory::HeapAllocator>);

  [[nodiscard]] auto in_buffer(const void *p, const u8 *buffer, usize length) -> bool
  {
    const auto *b = static_cast<const u8 *>(p);
    return b >= buffer && b < buffer + length;
  }

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
      add_test("ref_equality", [this] { return ref_equality(); });
      add_test("vec_shared_arena", [this] { return vec_shared_arena(); });
      add_test("vec_copy_shared_arena", [this] { return vec_copy_shared_arena(); });
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

    auto ref_equality() -> bool
    {
      alignas(16) u8 buffer_a[64];
      alignas(16) u8 buffer_b[64];
      memory::ArenaAllocator arena_a;
      memory::ArenaAllocator arena_b;
      arena_a.init(buffer_a, sizeof(buffer_a));
      arena_b.init(buffer_b, sizeof(buffer_b));

      const ArenaRef ref_a(arena_a);
      const ArenaRef ref_a2(arena_a);
      const ArenaRef ref_b(arena_b);
      if (!check(ref_a == ref_a2, "handles to the same arena compare equal"))
        return false;
      if (!check_not(ref_a == ref_b, "handles to different arenas compare unequal"))
        return false;

      void *p = ref_a2.alloc(16);
      return check(in_buffer(p, buffer_a, sizeof(buffer_a)), "handle allocates from the arena") &&
             check(arena_a.offset >= 16u, "allocation through a handle advances the arena");
    }

    auto vec_shared_arena() -> bool
    {
      alignas(16) u8 buffer[1024];
      memory::ArenaAllocator arena;
      arena.init(buffer, sizeof(buffer));
      const ArenaRef ref(arena);

      Vec<i32, ArenaRef> a(ArenaAdapter{ref});
      Vec<i32, ArenaRef> b(ArenaAdapter{ref});
      a.reserve(4);
      b.reserve(4);
      a.push_back(1);
      b.push_back(2);

      if (!check(a.get_allocator() == b.get_allocator(), "both vectors share one arena"))
        return false;
      if (!check(in_buffer(a.data(), buffer, sizeof(buffer)) && in_buffer(b.data(), buffer, sizeof(buffer)),
                 "both vectors allocate from the arena buffer"))
        return false;

      const auto *a_begin = reinterpret_cast<const u8 *>(a.data());
      const auto *b_begin = reinterpret_cast<const u8 *>(b.data());
      const auto *a_end = a_begin + a.capacity() * sizeof(i32);
      const auto *b_end = b_begin + b.capacity() * sizeof(i32);
      if (!check(a_end <= b_begin || b_end <= a_begin, "vectors receive disjoint memory"))
        return false;

      if (!check_eq(a[0], 1, "first vector keeps its element") || !check_eq(b[0], 2, "second vector keeps its element"))
        return false;

      return check(arena.offset >= 8u * sizeof(i32), "arena offset covers both vectors");
    }

    auto vec_copy_shared_arena() -> bool
    {
      alignas(16) u8 buffer[1024];
      memory::ArenaAllocator arena;
      arena.init(buffer, sizeof(buffer));
      const ArenaRef ref(arena);

      Vec<i32, ArenaRef> original(ArenaAdapter{ref});
      original.push_back(7);
      original.push_back(8);

      const usize offset_before_copy = arena.offset;
      Vec<i32, ArenaRef> copy = original;

      if (!check(copy.get_allocator() == original.get_allocator(), "copy uses the same arena"))
        return false;
      if (!check(arena.offset > offset_before_copy, "copy advances the shared arena"))
        return false;
      if (!check(in_buffer(copy.data(), buffer, sizeof(buffer)), "copy allocates from the arena buffer"))
        return false;
      if (!check_neq(copy.data(), original.data(), "copy has its own storage"))
        return false;

      copy[0] = 70;
      return check_eq(original[0], 7, "original unaffected by writes to the copy") &&
             check_eq(copy[1], 8, "copy keeps contents");
    }
  };

  const test::AutoRegister<ArenaBlock> _registered;
} // namespace
