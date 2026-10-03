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

  using ArenaHashMap = containers::HashMap<i32, i32, containers::Hash<i32>, containers::EqualTo<i32>, ArenaRef>;
  using ArenaHashSet = containers::HashSet<i32, containers::Hash<i32>, containers::EqualTo<i32>, ArenaRef>;

  template<typename A>
  concept ContainerAllocator = requires {
    typename BasicString<A>;
    typename CompactVec<i32, A>;
    typename TinyVec<i32, A>;
    typename SlotMap<i32, A>;
    typename containers::HashMap<i32, i32, containers::Hash<i32>, containers::EqualTo<i32>, A>;
    typename containers::HashSet<i32, containers::Hash<i32>, containers::EqualTo<i32>, A>;
  };

  template<typename T>
  concept DefaultBuildable = requires { T(); };

  // Auxid's own containers only take allocator handles, and with a handle
  // that has no default (an AllocatorRef) they can only be built from one.
  static_assert(!ContainerAllocator<memory::ArenaAllocator>);
  static_assert(ContainerAllocator<ArenaRef>);
  static_assert(ContainerAllocator<memory::HeapAllocator>);

  static_assert(!DefaultBuildable<BasicString<ArenaRef>>);
  static_assert(!DefaultBuildable<CompactVec<i32, ArenaRef>>);
  static_assert(!DefaultBuildable<TinyVec<i32, ArenaRef>>);
  static_assert(!DefaultBuildable<SlotMap<i32, ArenaRef>>);
  static_assert(!DefaultBuildable<ArenaHashMap>);
  static_assert(!DefaultBuildable<ArenaHashSet>);

  static_assert(DefaultBuildable<String>);
  static_assert(DefaultBuildable<CompactVec<i32>>);
  static_assert(DefaultBuildable<SlotMap<i32>>);
  static_assert(DefaultBuildable<HashMap<i32, i32>>);
  static_assert(DefaultBuildable<HashSet<i32>>);

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
      add_test("realloc_in_place", [this] { return realloc_in_place(); });
      add_test("realloc_copy", [this] { return realloc_copy(); });
      add_test("compact_vec_on_arena", [this] { return compact_vec_on_arena(); });
      add_test("compact_vec_copy_assign", [this] { return compact_vec_copy_assign(); });
      add_test("hash_map_on_arena", [this] { return hash_map_on_arena(); });
      add_test("slot_map_on_arena", [this] { return slot_map_on_arena(); });
      add_test("clear_after_handles_released", [this] { return clear_after_handles_released(); });
#if !defined(NDEBUG)
      add_test("debug_handle_count", [this] { return debug_handle_count(); });
#endif
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

    auto realloc_in_place() -> bool
    {
      alignas(16) u8 buffer[256];
      memory::ArenaAllocator arena;
      arena.init(buffer, sizeof(buffer));

      void *p = arena.alloc(16, 8);
      void *grown = arena.realloc(p, 16, 64, 8);
      if (!check_eq(grown, p, "last allocation grows in place") ||
          !check_eq(arena.offset, 64u, "offset follows growth"))
        return false;

      void *shrunk = arena.realloc(p, 64, 8, 8);
      if (!check_eq(shrunk, p, "last allocation shrinks in place") ||
          !check_eq(arena.offset, 8u, "offset follows shrink"))
        return false;

      void *fresh = arena.realloc(nullptr, 0, 16, 8);
      return check(in_buffer(fresh, buffer, sizeof(buffer)), "realloc(nullptr) allocates") &&
             check_eq(arena.offset, 24u, "realloc(nullptr) advances the offset");
    }

    auto realloc_copy() -> bool
    {
      alignas(16) u8 buffer[256];
      memory::ArenaAllocator arena;
      arena.init(buffer, sizeof(buffer));

      auto *first = static_cast<u8 *>(arena.alloc(16, 8));
      for (u8 i = 0; i < 16; ++i)
        first[i] = static_cast<u8>(i + 1);
      (void) arena.alloc(16, 8);
      const usize offset_before = arena.offset;

      auto *moved = static_cast<u8 *>(arena.realloc(first, 16, 32, 8));
      if (!check_neq(moved, first, "a block that is not last is copied"))
        return false;
      if (!check(in_buffer(moved, buffer, sizeof(buffer)), "copy comes from the arena"))
        return false;
      if (!check(arena.offset >= offset_before + 32, "copy advances the offset"))
        return false;
      for (u8 i = 0; i < 16; ++i)
      {
        if (moved[i] != i + 1 || first[i] != i + 1)
          return check(false, "old bytes copied and the old block left in place");
      }

      auto *shrunk = static_cast<u8 *>(arena.realloc(first, 16, 4, 8));
      return check_neq(shrunk, first, "shrinking a block that is not last copies too") &&
             check_eq(shrunk[3], static_cast<u8>(4), "shrink copies min(old, new) bytes");
    }

    auto compact_vec_on_arena() -> bool
    {
      alignas(16) static u8 buffer[8192];
      memory::ArenaAllocator arena;
      arena.init(buffer, sizeof(buffer));
      const ArenaRef ref(arena);

      CompactVec<i32, ArenaRef> v(ref);
      for (i32 i = 0; i < 200; ++i)
        v.push_back(i);
      if (!check(in_buffer(v.data(), buffer, sizeof(buffer)), "vector storage lives in the arena"))
        return false;
      for (i32 i = 0; i < 200; ++i)
      {
        if (v[static_cast<u32>(i)] != i)
          return check(false, "contents survive growth through realloc");
      }

      const usize offset_before_copy = arena.offset;
      const CompactVec<i32, ArenaRef> copy(v);
      return check(copy.get_allocator() == ref, "copy keeps the source's allocator") &&
             check(arena.offset > offset_before_copy, "copy allocates from the arena") &&
             check(in_buffer(copy.data(), buffer, sizeof(buffer)), "copy storage lives in the arena") &&
             check_eq(copy[199], 199, "copy keeps contents");
    }

    auto compact_vec_copy_assign() -> bool
    {
      alignas(16) u8 buffer_a[256];
      alignas(16) u8 buffer_b[256];
      memory::ArenaAllocator arena_a;
      memory::ArenaAllocator arena_b;
      arena_a.init(buffer_a, sizeof(buffer_a));
      arena_b.init(buffer_b, sizeof(buffer_b));
      const ArenaRef ref_a(arena_a);
      const ArenaRef ref_b(arena_b);

      CompactVec<i32, ArenaRef> dst(ref_a);
      CompactVec<i32, ArenaRef> src(ref_b);
      dst.push_back(1);
      src.push_back(2);
      src.push_back(3);
      dst = src;

      return check(dst.get_allocator() == ref_b, "copy assignment takes the source's allocator") &&
             check(in_buffer(dst.data(), buffer_b, sizeof(buffer_b)), "copied storage comes from the source's arena") &&
             check_eq(dst.size(), 2u, "copy assignment copies the size") && check_eq(dst[1], 3, "copy keeps contents");
    }

    auto hash_map_on_arena() -> bool
    {
      alignas(16) static u8 buffer[1 << 16];
      memory::ArenaAllocator arena;
      arena.init(buffer, sizeof(buffer));
      const ArenaRef ref(arena);

      ArenaHashMap map(ref);
      for (i32 i = 0; i < 300; ++i)
        map.insert(i, i * 10);
      if (!check(arena.offset > 0u, "map allocates from the arena"))
        return false;
      if (!check(map.get_allocator() == ref, "map holds the handle"))
        return false;

      const usize offset_before_copy = arena.offset;
      ArenaHashMap copy(map);
      if (!check(copy.get_allocator() == ref, "copy keeps the source's allocator") ||
          !check(arena.offset > offset_before_copy, "copy allocates from the arena"))
        return false;

      for (i32 i = 0; i < 300; ++i)
      {
        const i32 *a = map.find(i);
        const i32 *b = copy.find(i);
        if (a == nullptr || b == nullptr || *a != i * 10 || *b != i * 10)
          return check(false, "every key is found in the map and its copy");
      }

      ArenaHashSet set(16, ref);
      set.insert(42);
      return check(set.contains(42), "set on the arena works");
    }

    auto slot_map_on_arena() -> bool
    {
      alignas(16) u8 buffer[1024];
      memory::ArenaAllocator arena;
      arena.init(buffer, sizeof(buffer));
      const ArenaRef ref(arena);

      SlotMap<i32, ArenaRef> slots(ref);
      const auto key = slots.insert(5);
      const i32 *value = slots.get(key);
      return check(value != nullptr && *value == 5, "slot map stores the value") &&
             check(in_buffer(slots.data(), buffer, sizeof(buffer)), "slot map storage lives in the arena") &&
             check(slots.get_allocator() == ref, "slot map holds the handle");
    }

    auto clear_after_handles_released() -> bool
    {
      alignas(16) u8 buffer[256];
      memory::ArenaAllocator arena;
      arena.init(buffer, sizeof(buffer));
      {
        CompactVec<i32, ArenaRef> v{ArenaRef(arena)};
        v.push_back(1);
      }
      if (!check(arena.offset > 0u, "container used the arena"))
        return false;
      arena.clear();
      return check_eq(arena.offset, 0u, "clear succeeds once no handle is alive");
    }

#if !defined(NDEBUG)
    auto debug_handle_count() -> bool
    {
      alignas(16) u8 buffer_a[256];
      alignas(16) u8 buffer_b[256];
      memory::ArenaAllocator arena_a;
      memory::ArenaAllocator arena_b;
      arena_a.init(buffer_a, sizeof(buffer_a));
      arena_b.init(buffer_b, sizeof(buffer_b));

      if (!check_eq(arena_a.debug_live_handles(), 0u, "no handles yet"))
        return false;
      {
        const ArenaRef ref(arena_a);
        ArenaRef copy = ref;
        if (!check_eq(arena_a.debug_live_handles(), 2u, "construction and copy are counted"))
          return false;

        ArenaRef other(arena_b);
        other = copy;
        if (!check_eq(arena_a.debug_live_handles(), 3u, "assignment retains the new arena") ||
            !check_eq(arena_b.debug_live_handles(), 0u, "assignment releases the old arena"))
          return false;

        copy = ref;
        if (!check_eq(arena_a.debug_live_handles(), 3u, "assigning an equal handle keeps the count"))
          return false;

        {
          CompactVec<i32, ArenaRef> v(ref);
          const CompactVec<i32, ArenaRef> w(v);
          if (!check_eq(arena_a.debug_live_handles(), 5u, "containers hold handles, even empty ones"))
            return false;
        }
        if (!check_eq(arena_a.debug_live_handles(), 3u, "container destruction releases its handle"))
          return false;
      }
      if (!check_eq(arena_a.debug_live_handles(), 0u, "all handles released"))
        return false;

      (void) arena_a.alloc(16);
      arena_a.clear();
      return check_eq(arena_a.offset, 0u, "clear with no live handles succeeds");
    }
#endif
  };

  const test::AutoRegister<ArenaBlock> _registered;
} // namespace
