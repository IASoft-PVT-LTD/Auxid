// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

#include <bit>
#include <concepts>
#include <tuple>
#include <type_traits>

import auxid;
import auxid.test;

using namespace au;
using namespace au::containers;

template<typename T>
concept Hashable = requires(const T &v) {
  { Hash<T>{}(v) } -> std::convertible_to<u64>;
};

struct NoPadding
{
  u32 a;
  u32 b;
};

struct PaddedStruct
{
  u8 a;
  u32 b;
};

static_assert(Hashable<u32>, "u32 must be hashable via the integer mixer");
static_assert(Hashable<u64>, "u64 must be hashable via the integer mixer");
static_assert(Hashable<bool>, "bool must be hashable via the integer mixer");
static_assert(Hashable<char>, "char must be hashable via the integer mixer");
static_assert(Hashable<i32 *>, "Pointers must be hashable via the integer mixer");
static_assert(Hashable<String>, "String must be hashable via wyhash");
static_assert(Hashable<StringView>, "StringView must be hashable via wyhash");
static_assert(Hashable<NoPadding>, "Padding-free structs must hash via the generic wyhash fallback");

static_assert(!Hashable<PaddedStruct>, "Structs with padding MUST NOT satisfy the default Hash<T>; "
                                       "users must specialize Hash explicitly to avoid hashing "
                                       "uninitialised padding bytes.");

namespace
{
  struct HashBaseBlock final : test::Block
  {
    [[nodiscard]] auto get_name() const -> const char * override
    {
      return "containers::hash_base";
    }

    auto declare_tests() -> void override
    {
      add_test("int_mixer_disperses_sequential_inputs", [this] { return int_mixer_disperses_sequential_inputs(); });
      add_test("int_mixer_avalanche", [this] { return int_mixer_avalanche(); });
      add_test("string_hash_matches_stringview_hash", [this] { return string_hash_matches_stringview_hash(); });
      add_test("string_hash_changes_with_content", [this] { return string_hash_changes_with_content(); });
      add_test("string_hash_handles_empty", [this] { return string_hash_handles_empty(); });
      add_test("string_hash_long_input", [this] { return string_hash_long_input(); });
      add_test("struct_hash_works_for_padding_free", [this] { return struct_hash_works_for_padding_free(); });
      add_test("hash_combine_disperses", [this] { return hash_combine_disperses(); });
      add_test("pair_hash_distinguishes_field_order", [this] { return pair_hash_distinguishes_field_order(); });
      add_test("pair_hash_string_value", [this] { return pair_hash_string_value(); });
      add_test("tuple_hash_basic", [this] { return tuple_hash_basic(); });
      add_test("tuple_hash_empty_is_zero", [this] { return tuple_hash_empty_is_zero(); });
      add_test("span_bytes_hash_matches_hash_bytes", [this] { return span_bytes_hash_matches_hash_bytes(); });
      add_test("hash_combine_range_basic", [this] { return hash_combine_range_basic(); });
      add_test("seeded_int_hash_differs_by_seed", [this] { return seeded_int_hash_differs_by_seed(); });
      add_test("seeded_string_hash_differs_by_seed", [this] { return seeded_string_hash_differs_by_seed(); });
      add_test("seeded_struct_hash_differs_by_seed", [this] { return seeded_struct_hash_differs_by_seed(); });
      add_test("seeded_pair_hash_differs_by_seed", [this] { return seeded_pair_hash_differs_by_seed(); });
      add_test("seeded_tuple_hash_differs_by_seed", [this] { return seeded_tuple_hash_differs_by_seed(); });
      add_test("seedable_hasher_concept_holds", [this] { return seedable_hasher_concept_holds(); });
    }

    auto int_mixer_disperses_sequential_inputs() -> bool
    {
      Hash<u32> h;
      u64 a = h(0), b = h(1), c = h(2);
      return check_neq(a, b, "h(0) != h(1)") && check_neq(b, c, "h(1) != h(2)") && check_neq(a, c, "h(0) != h(2)");
    }

    auto int_mixer_avalanche() -> bool
    {
      Hash<u64> h;
      u64 x = 0x0123456789ABCDEFULL;
      u64 h1 = h(x);
      u64 h2 = h(x ^ 1ULL);
      u32 differing = static_cast<u32>(std::popcount(h1 ^ h2));
      return check(differing >= 8, "popcount(h1 ^ h2) >= 8");
    }

    auto string_hash_matches_stringview_hash() -> bool
    {
      Hash<String> hs;
      Hash<StringView> hv;

      String owned = "the quick brown fox";
      StringView view = "the quick brown fox";

      return check_eq(hs(owned), hv(view), "Hash<String>(owned) == Hash<StringView>(view)");
    }

    auto string_hash_changes_with_content() -> bool
    {
      Hash<StringView> h;
      return check_neq(h(StringView("foo")), h(StringView("bar")), "h(\"foo\") != h(\"bar\")") &&
             check_neq(h(StringView("foo")), h(StringView("food")), "h(\"foo\") != h(\"food\")") &&
             check_neq(h(StringView("foo")), h(StringView("oof")), "h(\"foo\") != h(\"oof\")");
    }

    auto string_hash_handles_empty() -> bool
    {
      Hash<StringView> h;
      return check_neq(h(StringView("")), h(StringView("a")), "h(\"\") != h(\"a\")");
    }

    auto string_hash_long_input() -> bool
    {
      String s;
      for (int i = 0; i < 256; ++i)
        s.push_back(static_cast<char>('a' + (i % 26)));
      Hash<StringView> h;
      u64 h1 = h(s);
      String s2 = s;
      s2.push_back('!');
      u64 h2 = h(s2);
      return check_neq(h1, h2, "h(s) != h(s + '!')");
    }

    auto struct_hash_works_for_padding_free() -> bool
    {
      Hash<NoPadding> h;
      NoPadding x{1, 2}, y{1, 2}, z{2, 1};
      return check_eq(h(x), h(y), "h({1,2}) == h({1,2})") && check_neq(h(x), h(z), "h({1,2}) != h({2,1})");
    }

    auto hash_combine_disperses() -> bool
    {
      u64 seed1 = hash_combine(42, 7);
      u64 seed2 = hash_combine(7, 42);
      return check_neq(seed1, seed2, "hash_combine is non-commutative");
    }

    auto pair_hash_distinguishes_field_order() -> bool
    {
      Hash<au::Pair<u32, u32>> h;
      au::Pair<u32, u32> a{1, 2};
      au::Pair<u32, u32> b{2, 1};
      au::Pair<u32, u32> c{1, 2};
      return check_eq(h(a), h(c), "h({1,2}) == h({1,2})") && check_neq(h(a), h(b), "h({1,2}) != h({2,1})");
    }

    auto pair_hash_string_value() -> bool
    {
      Hash<au::Pair<String, u64>> h;
      au::Pair<String, u64> a{String("hello"), 42};
      au::Pair<String, u64> b{String("hello"), 42};
      au::Pair<String, u64> c{String("hello"), 43};
      return check_eq(h(a), h(b), "h({\"hello\",42}) equality") && check_neq(h(a), h(c), "h(...,42) != h(...,43)");
    }

    auto tuple_hash_basic() -> bool
    {
      Hash<std::tuple<u32, u32, u32>> h;
      std::tuple<u32, u32, u32> a{1, 2, 3};
      std::tuple<u32, u32, u32> b{3, 2, 1};
      std::tuple<u32, u32, u32> c{1, 2, 3};
      return check_eq(h(a), h(c), "tuple hash equality") && check_neq(h(a), h(b), "tuple hash field order matters");
    }

    auto tuple_hash_empty_is_zero() -> bool
    {
      Hash<std::tuple<>> h;
      return check_eq(h(std::tuple<>{}), static_cast<u64>(0), "h(empty tuple) == 0");
    }

    auto span_bytes_hash_matches_hash_bytes() -> bool
    {
      const u8 buf[5] = {1, 2, 3, 4, 5};
      Hash<au::Span<const u8>> h;
      u64 via_span = h(au::Span<const u8>(buf, 5));
      u64 via_raw = hash_bytes(buf, 5);
      return check_eq(via_span, via_raw, "Hash<Span<const u8>> matches hash_bytes()");
    }

    auto hash_combine_range_basic() -> bool
    {
      u32 forward[3] = {10, 20, 30};
      u32 reverse[3] = {30, 20, 10};
      u64 a = hash_combine_range(0, forward, forward + 3);
      u64 b = hash_combine_range(0, reverse, reverse + 3);
      u64 c = hash_combine_range(0, forward, forward + 3);
      return check_eq(a, c, "hash_combine_range is deterministic") &&
             check_neq(a, b, "hash_combine_range is order-sensitive");
    }

    auto seeded_int_hash_differs_by_seed() -> bool
    {
      Hash<u64> h;
      const u64 v = 0xDEADBEEFCAFEBABEULL;
      const u64 h0 = h(v, 0ULL);
      const u64 h1 = h(v, 0xA5A5A5A5A5A5A5A5ULL);
      const u64 h2 = h(v, 0x5A5A5A5A5A5A5A5AULL);
      return check_neq(h0, h1, "h(v, 0) != h(v, A5..)") && check_neq(h1, h2, "h(v, A5..) != h(v, 5A..)") &&
             check_neq(h0, h2, "h(v, 0) != h(v, 5A..)") &&
             check_eq(h(v, 0xA5A5A5A5A5A5A5A5ULL), h1, "seeded hash is deterministic for a fixed seed");
    }

    auto seeded_string_hash_differs_by_seed() -> bool
    {
      Hash<StringView> h;
      const StringView sv("rigid c++ swiss table");
      const u64 h0 = h(sv, 0ULL);
      const u64 h1 = h(sv, 0x9E3779B97F4A7C15ULL);
      return check_neq(h0, h1, "wyhash output changes with the per-instance seed") &&
             check_eq(h(sv, 0x9E3779B97F4A7C15ULL), h1, "seeded string hash is deterministic for a fixed seed");
    }

    auto seeded_struct_hash_differs_by_seed() -> bool
    {
      Hash<NoPadding> h;
      NoPadding v{0x11223344u, 0x55667788u};
      const u64 h0 = h(v, 0ULL);
      const u64 h1 = h(v, 0xCAFEBABE12345678ULL);
      return check_neq(h0, h1, "padding-free struct hash uses seed");
    }

    auto seeded_pair_hash_differs_by_seed() -> bool
    {
      Hash<au::Pair<u32, u64>> h;
      au::Pair<u32, u64> v{42u, 0xFEEDFACEDEADBEEFULL};
      const u64 h0 = h(v, 0ULL);
      const u64 h1 = h(v, 0xC0FFEEC0FFEEC0FFULL);
      return check_neq(h0, h1, "pair hash threads seed through both halves");
    }

    auto seeded_tuple_hash_differs_by_seed() -> bool
    {
      Hash<std::tuple<u32, u32, u32>> h;
      std::tuple<u32, u32, u32> v{1u, 2u, 3u};
      const u64 h0 = h(v, 0ULL);
      const u64 h1 = h(v, 0xC001D00DC001D00DULL);
      return check_neq(h0, h1, "tuple hash threads seed through every field");
    }

    auto seedable_hasher_concept_holds() -> bool
    {
      using namespace au::containers;
      static_assert(SeedableHasher<Hash<u32>, u32>, "Hash<u32> must be SeedableHasher");
      static_assert(SeedableHasher<Hash<u64>, u64>, "Hash<u64> must be SeedableHasher");
      static_assert(SeedableHasher<Hash<StringView>, StringView>, "Hash<StringView> must be SeedableHasher");
      static_assert(SeedableHasher<Hash<String>, StringView>, "Hash<String> must be SeedableHasher transparently");
      static_assert(SeedableHasher<Hash<NoPadding>, NoPadding>, "Hash<NoPadding> must be SeedableHasher");
      return check(true, "static_asserts above");
    }
  };

  const test::AutoRegister<HashBaseBlock> _registered;
} // namespace
