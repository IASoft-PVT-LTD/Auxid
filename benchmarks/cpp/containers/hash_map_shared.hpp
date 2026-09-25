// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <benchmark/benchmark.h>

// Include after `import auxid;` in the translation unit.
// Avoid <random> here: MinGW GCC + C++ modules re-imports std headers and breaks.

namespace au_bench_hash
{
  inline constexpr std::uint32_t RANGE_MIN = 1u << 9;
  inline constexpr std::uint32_t RANGE_MAX = 1u << 16;
  inline constexpr au::usize PROBE_ORDER_SIZE = 1u << 14;
  inline constexpr au::usize LOOKUP_BATCH = 64;
  inline constexpr float STD_MAX_LOAD_FACTOR = 0.875f;

  struct Lcg
  {
    std::uint64_t state;

    constexpr explicit Lcg(std::uint64_t seed) noexcept : state(seed)
    {
    }

    constexpr auto next_u32() noexcept -> std::uint32_t
    {
      state = state * 6364136223846793005ULL + 1442695040888963407ULL;
      return static_cast<std::uint32_t>(state >> 32);
    }

    auto next_u64() noexcept -> std::uint64_t
    {
      return static_cast<std::uint64_t>(next_u32()) | (static_cast<std::uint64_t>(next_u32()) << 32);
    }

    auto next_bounded(std::uint32_t bound) noexcept -> std::uint32_t
    {
      return bound == 0 ? 0 : next_u32() % bound;
    }
  };

  struct AuI32Hash
  {
    [[nodiscard]] auto operator()(au::i32 k) const noexcept -> au::usize
    {
      au::containers::Hash<au::i32> h{};
      return static_cast<au::usize>(h(k));
    }
  };

  struct AuU64Hash
  {
    [[nodiscard]] auto operator()(au::u64 k) const noexcept -> au::usize
    {
      au::containers::Hash<au::u64> h{};
      return static_cast<au::usize>(h(k));
    }
  };

  struct AuStringStdHash
  {
    [[nodiscard]] auto operator()(const au::String &s) const noexcept -> au::usize
    {
      au::containers::Hash<au::String> h{};
      return static_cast<au::usize>(h(au::StringView{s.data(), s.size()}));
    }
  };

  struct AuStringStdEq
  {
    [[nodiscard]] auto operator()(const au::String &a, const au::String &b) const noexcept -> bool
    {
      return au::StringView{a.data(), a.size()} == au::StringView{b.data(), b.size()};
    }
  };

  template<typename K, typename V>
  using StdPairAlloc = au::memory::StdAllocatorAdapter<std::pair<const K, V>, au::memory::HeapAllocator>;

  template<typename K, typename V> using StdUnorderedMapStock = std::unordered_map<K, V>;

  template<typename K, typename V>
  using StdUnorderedMapSameAlloc = std::unordered_map<K, V, std::hash<K>, std::equal_to<K>, StdPairAlloc<K, V>>;

  template<typename K, typename V, typename Hash, typename Eq = std::equal_to<K>>
  using StdUnorderedMapSameHashAlloc = std::unordered_map<K, V, Hash, Eq, StdPairAlloc<K, V>>;

  template<typename K, typename V, typename Hash, typename Eq>
  using StdUnorderedMapStringStock = std::unordered_map<K, V, Hash, Eq>;

  template<typename K, typename V, typename Hash, typename Eq>
  using StdUnorderedMapStringSameAlloc = std::unordered_map<K, V, Hash, Eq, StdPairAlloc<K, V>>;

  struct LookupProbePlan
  {
    std::vector<au::usize> key_indices;
    std::vector<bool> is_hit;
  };

  inline auto fisher_yates_shuffle(std::vector<au::usize> &indices, std::uint64_t seed) -> void
  {
    Lcg rng{seed};
    for (au::usize i = indices.size(); i > 1; --i)
    {
      const au::usize j = static_cast<au::usize>(rng.next_bounded(static_cast<std::uint32_t>(i)));
      au::usize tmp = indices[i - 1];
      indices[i - 1] = indices[j];
      indices[j] = tmp;
    }
  }

  inline auto make_shuffled_indices(au::usize n, std::uint64_t seed) -> std::vector<au::usize>
  {
    std::vector<au::usize> indices(n);
    for (au::usize i = 0; i < n; ++i)
    {
      indices[i] = i;
    }
    fisher_yates_shuffle(indices, seed);
    return indices;
  }

  template<typename T>
  inline auto vector_contains(const std::vector<T> &values, const T &value) -> bool
  {
    for (const auto &v : values)
    {
      if (v == value)
      {
        return true;
      }
    }
    return false;
  }

  inline auto make_keys_i32_unique(au::usize n) -> std::vector<au::i32>
  {
    Lcg rng{0xA0C1D54Eu};
    std::vector<au::i32> keys;
    keys.reserve(n);
    while (keys.size() < n)
    {
      const au::i32 k = static_cast<au::i32>(rng.next_bounded(1'000'000'000u) + 1u);
      if (!vector_contains(keys, k))
      {
        keys.push_back(k);
      }
    }
    return keys;
  }

  inline auto make_miss_keys_i32(au::usize n) -> std::vector<au::i32>
  {
    Lcg rng{0xBADC0FFEu};
    std::vector<au::i32> keys;
    keys.reserve(n);
    while (keys.size() < n)
    {
      const au::i32 k = static_cast<au::i32>(rng.next_bounded(1'000'000'000u) + 1'000'000'001);
      if (!vector_contains(keys, k))
      {
        keys.push_back(k);
      }
    }
    return keys;
  }

  inline auto make_keys_u64_unique(au::usize n) -> std::vector<au::u64>
  {
    Lcg rng{0xC0FFEE42u};
    std::vector<au::u64> keys;
    keys.reserve(n);
    while (keys.size() < n)
    {
      const au::u64 k = (rng.next_u64() % 1'000'000'000ULL) + 1ULL;
      if (!vector_contains(keys, k))
      {
        keys.push_back(k);
      }
    }
    return keys;
  }

  inline auto make_keys_string_unique(au::usize n) -> std::vector<std::string>
  {
    Lcg rng{0x5EEDB055u};
    std::vector<std::string> keys;
    keys.reserve(n);
    while (keys.size() < n)
    {
      const auto len = static_cast<au::usize>(rng.next_bounded(22u) + 3u);
      std::string s;
      s.resize(len);
      for (au::usize k = 0; k < len; ++k)
      {
        s[k] = static_cast<char>('a' + static_cast<int>(rng.next_bounded(26u)));
      }
      if (!vector_contains(keys, s))
      {
        keys.push_back(std::move(s));
      }
    }
    return keys;
  }

  inline auto make_miss_keys_string_unique(au::usize n) -> std::vector<std::string>
  {
    Lcg rng{0xDEADBEEFu};
    std::vector<std::string> keys;
    keys.reserve(n);
    while (keys.size() < n)
    {
      const auto len = static_cast<au::usize>(rng.next_bounded(22u) + 3u);
      std::string s;
      s.push_back('!');
      s.resize(len);
      for (au::usize k = 1; k < len; ++k)
      {
        s[k] = static_cast<char>('A' + static_cast<int>(rng.next_bounded(26u)));
      }
      if (!vector_contains(keys, s))
      {
        keys.push_back(std::move(s));
      }
    }
    return keys;
  }

  inline auto to_au_strings(const std::vector<std::string> &src) -> std::vector<au::String>
  {
    std::vector<au::String> out;
    out.reserve(src.size());
    for (const auto &s : src)
    {
      out.emplace_back(s.data(), s.size());
    }
    return out;
  }

  inline auto make_lookup_probe_plan(au::usize n, unsigned hit_percent, std::uint64_t seed) -> LookupProbePlan
  {
    LookupProbePlan plan{};
    plan.key_indices.resize(PROBE_ORDER_SIZE);
    plan.is_hit.resize(PROBE_ORDER_SIZE);

    const au::usize hit_slots = (PROBE_ORDER_SIZE * hit_percent) / 100u;
    for (au::usize i = 0; i < PROBE_ORDER_SIZE; ++i)
    {
      plan.is_hit[i] = i < hit_slots;
      plan.key_indices[i] = i % n;
    }

    std::vector<au::usize> order(PROBE_ORDER_SIZE);
    for (au::usize i = 0; i < PROBE_ORDER_SIZE; ++i)
    {
      order[i] = i;
    }
    fisher_yates_shuffle(order, seed);

    LookupProbePlan shuffled{};
    shuffled.key_indices.resize(PROBE_ORDER_SIZE);
    shuffled.is_hit.resize(PROBE_ORDER_SIZE);
    for (au::usize i = 0; i < PROBE_ORDER_SIZE; ++i)
    {
      const au::usize src = order[i];
      shuffled.is_hit[i] = plan.is_hit[src];
      shuffled.key_indices[i] = plan.key_indices[src];
    }
    return shuffled;
  }

  inline auto make_adv_auxid_collision_keys(au::usize n) -> std::vector<au::u64>
  {
    std::vector<au::u64> keys;
    keys.reserve(n);
    for (au::usize i = 0; i < n; ++i)
    {
      keys.push_back(static_cast<au::u64>(i) * 128ULL ^ 0xDEADC0DE12345678ULL);
    }
    return keys;
  }

  inline auto make_adv_std_identity_collision_keys(au::usize n) -> std::vector<au::u64>
  {
    std::vector<au::u64> keys;
    keys.reserve(n);
    for (au::usize i = 0; i < n; ++i)
    {
      keys.push_back(static_cast<au::u64>(i) << 16);
    }
    return keys;
  }

  template<typename Map, typename Key, typename GetValue>
  inline auto run_batched_lookup(benchmark::State &state, Map &map, const std::vector<Key> &hit_keys,
                                 const std::vector<Key> &miss_keys, const LookupProbePlan &plan, GetValue get_value)
      -> void
  {
    au::usize slot = 0;
    for (auto _ : state)
    {
      au::i64 acc = 0;
      for (au::usize b = 0; b < LOOKUP_BATCH; ++b)
      {
        const au::usize pos = (slot + b) & (PROBE_ORDER_SIZE - 1);
        const au::usize idx = plan.key_indices[pos] % hit_keys.size();
        if (plan.is_hit[pos])
        {
          acc += get_value(map, hit_keys[idx]);
        }
        else
        {
          acc += get_value(map, miss_keys[idx]);
        }
      }
      slot = (slot + LOOKUP_BATCH) & (PROBE_ORDER_SIZE - 1);
      benchmark::DoNotOptimize(acc);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) * static_cast<std::int64_t>(LOOKUP_BATCH));
  }

#define AU_BENCH_HASH_RANGE(BM) BENCHMARK(BM)->Range(au_bench_hash::RANGE_MIN, au_bench_hash::RANGE_MAX)

} // namespace au_bench_hash
