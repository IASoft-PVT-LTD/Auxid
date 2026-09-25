// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>

#include <benchmark/benchmark.h>

import auxid;

#include "hash_map_shared.hpp"

using namespace au;
using namespace au_bench_hash;

namespace
{
  template<typename Map, typename Key>
  inline auto insert_i32(Map &map, const Key &key, i32 val) -> void
  {
    if constexpr (requires { map.emplace(key, val); })
    {
      map.emplace(key, val);
    }
    else
    {
      map.insert(key, val);
    }
  }

  template<typename Map, typename Key>
  inline auto lookup_value_i32(const Map &map, const Key &key) -> i64
  {
    if constexpr (requires { map.find(key); })
    {
      if constexpr (requires { map.find(key)->second; })
      {
        const auto it = map.find(key);
        return it != map.end() ? static_cast<i64>(it->second) : 0;
      }
      else
      {
        const auto *v = map.find(key);
        return v ? static_cast<i64>(*v) : 0;
      }
    }
  }

  template<typename Map>
  inline auto configure_std_map(Map &map) -> void
  {
    if constexpr (requires { map.max_load_factor(1.0f); })
    {
      map.max_load_factor(STD_MAX_LOAD_FACTOR);
    }
  }

  template<typename Map, typename Key>
  inline auto insert_until_size(Map &map, const std::vector<Key> &keys, usize target_size) -> void
  {
    usize key_idx = 0;
    while (map.size() < target_size)
    {
      insert_i32(map, keys[key_idx % keys.size()], static_cast<i32>(map.size()));
      ++key_idx;
    }
  }

  // ---------------------------------------------------------------------------
  // Insert (i32)
  // ---------------------------------------------------------------------------

  template<typename Map>
  static auto run_insert_i32(benchmark::State &state, bool reserve) -> void
  {
    const auto n = static_cast<usize>(state.range(0));
    const auto keys = make_keys_i32_unique(n);

    for (auto _ : state)
    {
      Map map;
      configure_std_map(map);
      if (reserve)
      {
        map.reserve(n);
      }
      for (usize i = 0; i < n; ++i)
      {
        insert_i32(map, keys[i], static_cast<i32>(i));
      }
      benchmark::DoNotOptimize(map);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
  }

  static auto run_insert_i32_au(benchmark::State &state, bool reserve) -> void
  {
    const auto n = static_cast<usize>(state.range(0));
    const auto keys = make_keys_i32_unique(n);

    for (auto _ : state)
    {
      HashMap<i32, i32> map;
      if (reserve)
      {
        map.reserve(n);
      }
      for (usize i = 0; i < n; ++i)
      {
        map.insert(keys[i], static_cast<i32>(i));
      }
      benchmark::DoNotOptimize(map);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
  }

  static auto BM_AuHashMap_Insert_i32(benchmark::State &state) -> void
  {
    run_insert_i32_au(state, true);
  }
  AU_BENCH_HASH_RANGE(BM_AuHashMap_Insert_i32);

  static auto BM_AuHashMap_Insert_i32_NoReserve(benchmark::State &state) -> void
  {
    run_insert_i32_au(state, false);
  }
  AU_BENCH_HASH_RANGE(BM_AuHashMap_Insert_i32_NoReserve);

  static auto BM_StdUnorderedMap_Insert_i32_StdStock(benchmark::State &state) -> void
  {
    run_insert_i32<StdUnorderedMapStock<i32, i32>>(state, true);
  }
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Insert_i32_StdStock);

  static auto BM_StdUnorderedMap_Insert_i32_StdStock_NoReserve(benchmark::State &state) -> void
  {
    run_insert_i32<StdUnorderedMapStock<i32, i32>>(state, false);
  }
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Insert_i32_StdStock_NoReserve);

  static auto BM_StdUnorderedMap_Insert_i32_StdSameAlloc(benchmark::State &state) -> void
  {
    run_insert_i32<StdUnorderedMapSameAlloc<i32, i32>>(state, true);
  }
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Insert_i32_StdSameAlloc);

  static auto BM_StdUnorderedMap_Insert_i32_StdSameAlloc_NoReserve(benchmark::State &state) -> void
  {
    run_insert_i32<StdUnorderedMapSameAlloc<i32, i32>>(state, false);
  }
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Insert_i32_StdSameAlloc_NoReserve);

  static auto BM_StdUnorderedMap_Insert_i32_StdSameHashAlloc(benchmark::State &state) -> void
  {
    run_insert_i32<StdUnorderedMapSameHashAlloc<i32, i32, AuI32Hash>>(state, true);
  }
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Insert_i32_StdSameHashAlloc);

  static auto BM_StdUnorderedMap_Insert_i32_StdSameHashAlloc_NoReserve(benchmark::State &state) -> void
  {
    run_insert_i32<StdUnorderedMapSameHashAlloc<i32, i32, AuI32Hash>>(state, false);
  }
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Insert_i32_StdSameHashAlloc_NoReserve);

  // ---------------------------------------------------------------------------
  // Lookup (i32)
  // ---------------------------------------------------------------------------

  template<typename Map>
  static auto run_lookup_i32(benchmark::State &state, unsigned hit_percent) -> void
  {
    const auto n = static_cast<usize>(state.range(0));
    const auto keys = make_keys_i32_unique(n);
    const auto miss_keys = make_miss_keys_i32(n);
    const auto plan = make_lookup_probe_plan(n, hit_percent, 0x1A2B3C4Du);

    Map map;
    configure_std_map(map);
    map.reserve(n);
    for (usize i = 0; i < n; ++i)
    {
      insert_i32(map, keys[i], static_cast<i32>(i));
    }

    run_batched_lookup(
        state, map, keys, miss_keys, plan,
        [](const Map &m, const i32 &k) -> i64 { return lookup_value_i32(m, k); });
  }

  static auto run_lookup_i32_au(benchmark::State &state, unsigned hit_percent) -> void
  {
    const auto n = static_cast<usize>(state.range(0));
    const auto keys = make_keys_i32_unique(n);
    const auto miss_keys = make_miss_keys_i32(n);
    const auto plan = make_lookup_probe_plan(n, hit_percent, 0x1A2B3C4Du);

    HashMap<i32, i32> map;
    map.reserve(n);
    for (usize i = 0; i < n; ++i)
    {
      map.insert(keys[i], static_cast<i32>(i));
    }

    run_batched_lookup(
        state, map, keys, miss_keys, plan,
        [](const HashMap<i32, i32> &m, const i32 &k) -> i64 {
          const auto *v = m.find(k);
          return v ? static_cast<i64>(*v) : 0;
        });
  }

#define DEFINE_LOOKUP_I32_BENCH(NAME, HIT)                                                                             \
  static auto BM_AuHashMap_Lookup_i32_##NAME(benchmark::State &state) -> void                                        \
  {                                                                                                                    \
    run_lookup_i32_au(state, HIT);                                                                                     \
  }                                                                                                                    \
  AU_BENCH_HASH_RANGE(BM_AuHashMap_Lookup_i32_##NAME);                                                                 \
  static auto BM_StdUnorderedMap_Lookup_i32_StdStock_##NAME(benchmark::State &state) -> void                         \
  {                                                                                                                    \
    run_lookup_i32<StdUnorderedMapStock<i32, i32>>(state, HIT);                                                      \
  }                                                                                                                    \
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Lookup_i32_StdStock_##NAME);                                                  \
  static auto BM_StdUnorderedMap_Lookup_i32_StdSameAlloc_##NAME(benchmark::State &state) -> void                     \
  {                                                                                                                    \
    run_lookup_i32<StdUnorderedMapSameAlloc<i32, i32>>(state, HIT);                                                  \
  }                                                                                                                    \
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Lookup_i32_StdSameAlloc_##NAME);                                              \
  static auto BM_StdUnorderedMap_Lookup_i32_StdSameHashAlloc_##NAME(benchmark::State &state) -> void                \
  {                                                                                                                    \
    run_lookup_i32<StdUnorderedMapSameHashAlloc<i32, i32, AuI32Hash>>(state, HIT);                                   \
  }                                                                                                                    \
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Lookup_i32_StdSameHashAlloc_##NAME)

  DEFINE_LOOKUP_I32_BENCH(Hit100, 100);
  DEFINE_LOOKUP_I32_BENCH(Hit50, 50);
  DEFINE_LOOKUP_I32_BENCH(Hit0, 0);

#undef DEFINE_LOOKUP_I32_BENCH

  // ---------------------------------------------------------------------------
  // Erase (i32) — fixture with manual timing
  // ---------------------------------------------------------------------------

  class I32EraseFixture : public benchmark::Fixture
  {
  public:
    void SetUp(const benchmark::State &state) override
    {
      n = static_cast<usize>(state.range(0));
      keys = make_keys_i32_unique(n);
      erase_order = make_shuffled_indices(n, 0xE5A5E000u);
    }

    std::vector<i32> keys;
    std::vector<usize> erase_order;
    usize n{};
  };

  BENCHMARK_DEFINE_F(I32EraseFixture, AuHashMap_Erase_i32)(benchmark::State &state)
  {
    for (auto _ : state)
    {
      HashMap<i32, i32> map;
      map.reserve(n);
      for (usize i = 0; i < n; ++i)
      {
        map.insert(keys[i], static_cast<i32>(i));
      }

      const auto start = std::chrono::steady_clock::now();
      for (usize i = 0; i < n; ++i)
      {
        const bool ok = map.erase(keys[erase_order[i]]);
        benchmark::DoNotOptimize(ok);
      }
      const auto end = std::chrono::steady_clock::now();
      state.SetIterationTime(std::chrono::duration<double>(end - start).count());
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
  }
  BENCHMARK_REGISTER_F(I32EraseFixture, AuHashMap_Erase_i32)
      ->UseManualTime()
      ->Range(RANGE_MIN, RANGE_MAX);

  template<typename Map>
  static auto erase_i32_fixture_body(benchmark::State &state, const std::vector<i32> &keys,
                                     const std::vector<usize> &erase_order, usize n) -> void
  {
    for (auto _ : state)
    {
      Map map;
      configure_std_map(map);
      map.reserve(n);
      for (usize i = 0; i < n; ++i)
      {
        insert_i32(map, keys[i], static_cast<i32>(i));
      }

      const auto start = std::chrono::steady_clock::now();
      for (usize i = 0; i < n; ++i)
      {
        const auto removed = map.erase(keys[erase_order[i]]);
        benchmark::DoNotOptimize(removed);
      }
      const auto end = std::chrono::steady_clock::now();
      state.SetIterationTime(std::chrono::duration<double>(end - start).count());
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
  }

  BENCHMARK_DEFINE_F(I32EraseFixture, StdUnorderedMap_Erase_i32_StdStock)(benchmark::State &state)
  {
    erase_i32_fixture_body<StdUnorderedMapStock<i32, i32>>(state, keys, erase_order, n);
  }
  BENCHMARK_REGISTER_F(I32EraseFixture, StdUnorderedMap_Erase_i32_StdStock)
      ->UseManualTime()
      ->Range(RANGE_MIN, RANGE_MAX);

  BENCHMARK_DEFINE_F(I32EraseFixture, StdUnorderedMap_Erase_i32_StdSameAlloc)(benchmark::State &state)
  {
    erase_i32_fixture_body<StdUnorderedMapSameAlloc<i32, i32>>(state, keys, erase_order, n);
  }
  BENCHMARK_REGISTER_F(I32EraseFixture, StdUnorderedMap_Erase_i32_StdSameAlloc)
      ->UseManualTime()
      ->Range(RANGE_MIN, RANGE_MAX);

  BENCHMARK_DEFINE_F(I32EraseFixture, StdUnorderedMap_Erase_i32_StdSameHashAlloc)(benchmark::State &state)
  {
    erase_i32_fixture_body<StdUnorderedMapSameHashAlloc<i32, i32, AuI32Hash>>(state, keys, erase_order, n);
  }
  BENCHMARK_REGISTER_F(I32EraseFixture, StdUnorderedMap_Erase_i32_StdSameHashAlloc)
      ->UseManualTime()
      ->Range(RANGE_MIN, RANGE_MAX);

  // ---------------------------------------------------------------------------
  // Mixed (i32)
  // ---------------------------------------------------------------------------

  template<typename Map>
  static auto run_mixed_i32(benchmark::State &state) -> void
  {
    const auto n = static_cast<usize>(state.range(0));
    const auto keys = make_keys_i32_unique(n);

    for (auto _ : state)
    {
      Map map;
      configure_std_map(map);
      map.reserve(n / 2);
      for (usize i = 0; i < n; ++i)
      {
        insert_i32(map, keys[i], static_cast<i32>(i));
        if ((i & 0x3u) == 0x3u && i >= 4)
        {
          const auto removed = map.erase(keys[i - 4]);
          benchmark::DoNotOptimize(removed);
        }
        const auto val = lookup_value_i32(map, keys[i >> 1]);
        benchmark::DoNotOptimize(val);
      }
      benchmark::DoNotOptimize(map);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
  }

  static auto run_mixed_i32_au(benchmark::State &state) -> void
  {
    const auto n = static_cast<usize>(state.range(0));
    const auto keys = make_keys_i32_unique(n);

    for (auto _ : state)
    {
      HashMap<i32, i32> map;
      map.reserve(n / 2);
      for (usize i = 0; i < n; ++i)
      {
        map.insert(keys[i], static_cast<i32>(i));
        if ((i & 0x3u) == 0x3u && i >= 4)
        {
          const bool ok = map.erase(keys[i - 4]);
          benchmark::DoNotOptimize(ok);
        }
        const auto *v = map.find(keys[i >> 1]);
        benchmark::DoNotOptimize(v);
      }
      benchmark::DoNotOptimize(map);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
  }

  static auto BM_AuHashMap_Mixed_i32(benchmark::State &state) -> void
  {
    run_mixed_i32_au(state);
  }
  AU_BENCH_HASH_RANGE(BM_AuHashMap_Mixed_i32);

  static auto BM_StdUnorderedMap_Mixed_i32_StdStock(benchmark::State &state) -> void
  {
    run_mixed_i32<StdUnorderedMapStock<i32, i32>>(state);
  }
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Mixed_i32_StdStock);

  static auto BM_StdUnorderedMap_Mixed_i32_StdSameAlloc(benchmark::State &state) -> void
  {
    run_mixed_i32<StdUnorderedMapSameAlloc<i32, i32>>(state);
  }
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Mixed_i32_StdSameAlloc);

  static auto BM_StdUnorderedMap_Mixed_i32_StdSameHashAlloc(benchmark::State &state) -> void
  {
    run_mixed_i32<StdUnorderedMapSameHashAlloc<i32, i32, AuI32Hash>>(state);
  }
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Mixed_i32_StdSameHashAlloc);

  // ---------------------------------------------------------------------------
  // Iterate (i32)
  // ---------------------------------------------------------------------------

  template<typename Map>
  static auto run_iterate_i32(benchmark::State &state) -> void
  {
    const auto n = static_cast<usize>(state.range(0));
    const auto keys = make_keys_i32_unique(n);

    Map map;
    configure_std_map(map);
    map.reserve(n);
    insert_until_size(map, keys, n);

    for (auto _ : state)
    {
      i64 sum = 0;
      for (const auto &p : map)
      {
        sum += static_cast<i64>(p.second);
      }
      benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
  }

  static auto run_iterate_i32_au(benchmark::State &state) -> void
  {
    const auto n = static_cast<usize>(state.range(0));
    const auto keys = make_keys_i32_unique(n);

    HashMap<i32, i32> map;
    map.reserve(n);
    for (usize i = 0; i < n; ++i)
    {
      map.insert(keys[i], static_cast<i32>(i));
    }

    for (auto _ : state)
    {
      i64 sum = 0;
      for (const auto &p : map)
      {
        sum += static_cast<i64>(p.second);
      }
      benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
  }

  static auto BM_AuHashMap_Iterate_i32(benchmark::State &state) -> void
  {
    run_iterate_i32_au(state);
  }
  AU_BENCH_HASH_RANGE(BM_AuHashMap_Iterate_i32);

  static auto BM_StdUnorderedMap_Iterate_i32_StdStock(benchmark::State &state) -> void
  {
    run_iterate_i32<StdUnorderedMapStock<i32, i32>>(state);
  }
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Iterate_i32_StdStock);

  static auto BM_StdUnorderedMap_Iterate_i32_StdSameAlloc(benchmark::State &state) -> void
  {
    run_iterate_i32<StdUnorderedMapSameAlloc<i32, i32>>(state);
  }
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Iterate_i32_StdSameAlloc);

  static auto BM_StdUnorderedMap_Iterate_i32_StdSameHashAlloc(benchmark::State &state) -> void
  {
    run_iterate_i32<StdUnorderedMapSameHashAlloc<i32, i32, AuI32Hash>>(state);
  }
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Iterate_i32_StdSameHashAlloc);

  // ---------------------------------------------------------------------------
  // High-collision diagnostics (single implementation each)
  // ---------------------------------------------------------------------------

  static auto BM_HighCollision_AdvAuxid_Lookup_u64(benchmark::State &state) -> void
  {
    const auto n = static_cast<usize>(state.range(0));
    const auto keys = make_adv_auxid_collision_keys(n);
    const auto miss_keys = make_adv_std_identity_collision_keys(n);
    const auto plan = make_lookup_probe_plan(n, 100, 0xC0111DEu);

    HashMap<u64, i32> map;
    map.reserve(n);
    for (usize i = 0; i < n; ++i)
    {
      map.insert(keys[i], static_cast<i32>(i));
    }

    run_batched_lookup(
        state, map, keys, miss_keys, plan,
        [](const HashMap<u64, i32> &m, const u64 &k) -> i64 {
          const auto *v = m.find(k);
          return v ? static_cast<i64>(*v) : 0;
        });
  }
  AU_BENCH_HASH_RANGE(BM_HighCollision_AdvAuxid_Lookup_u64);

  static auto BM_HighCollision_AdvStdHashIdentity_Lookup_u64(benchmark::State &state) -> void
  {
    const auto n = static_cast<usize>(state.range(0));
    const auto keys = make_adv_std_identity_collision_keys(n);
    const auto miss_keys = make_adv_auxid_collision_keys(n);
    const auto plan = make_lookup_probe_plan(n, 100, 0xC0111DEu);

    StdUnorderedMapStock<u64, i32> map;
    configure_std_map(map);
    map.reserve(n);
    for (usize i = 0; i < n; ++i)
    {
      map.emplace(keys[i], static_cast<i32>(i));
    }

    run_batched_lookup(
        state, map, keys, miss_keys, plan,
        [](const StdUnorderedMapStock<u64, i32> &m, const u64 &k) -> i64 {
          return lookup_value_i32(m, k);
        });
  }
  AU_BENCH_HASH_RANGE(BM_HighCollision_AdvStdHashIdentity_Lookup_u64);

  // ---------------------------------------------------------------------------
  // Insert (String)
  // ---------------------------------------------------------------------------

  template<typename Map>
  static auto run_insert_string(benchmark::State &state, bool reserve) -> void
  {
    const auto n = static_cast<usize>(state.range(0));
    const auto keys = to_au_strings(make_keys_string_unique(n));

    for (auto _ : state)
    {
      Map map;
      configure_std_map(map);
      if (reserve)
      {
        map.reserve(n);
      }
      for (usize i = 0; i < n; ++i)
      {
        insert_i32(map, keys[i], static_cast<i32>(i));
      }
      benchmark::DoNotOptimize(map);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
  }

  static auto run_insert_string_au(benchmark::State &state, bool reserve) -> void
  {
    const auto n = static_cast<usize>(state.range(0));
    const auto keys = to_au_strings(make_keys_string_unique(n));

    for (auto _ : state)
    {
      HashMap<String, i32> map;
      if (reserve)
      {
        map.reserve(n);
      }
      for (usize i = 0; i < n; ++i)
      {
        map.insert(keys[i], static_cast<i32>(i));
      }
      benchmark::DoNotOptimize(map);
      benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(n));
  }

  static auto BM_AuHashMap_Insert_String(benchmark::State &state) -> void
  {
    run_insert_string_au(state, true);
  }
  AU_BENCH_HASH_RANGE(BM_AuHashMap_Insert_String);

  static auto BM_AuHashMap_Insert_String_NoReserve(benchmark::State &state) -> void
  {
    run_insert_string_au(state, false);
  }
  AU_BENCH_HASH_RANGE(BM_AuHashMap_Insert_String_NoReserve);

  static auto BM_StdUnorderedMap_Insert_String_StdStock(benchmark::State &state) -> void
  {
    run_insert_string<StdUnorderedMapStringStock<String, i32, AuStringStdHash, AuStringStdEq>>(state, true);
  }
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Insert_String_StdStock);

  static auto BM_StdUnorderedMap_Insert_String_StdStock_NoReserve(benchmark::State &state) -> void
  {
    run_insert_string<StdUnorderedMapStringStock<String, i32, AuStringStdHash, AuStringStdEq>>(state, false);
  }
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Insert_String_StdStock_NoReserve);

  static auto BM_StdUnorderedMap_Insert_String_StdSameAlloc(benchmark::State &state) -> void
  {
    run_insert_string<StdUnorderedMapStringSameAlloc<String, i32, AuStringStdHash, AuStringStdEq>>(state, true);
  }
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Insert_String_StdSameAlloc);

  static auto BM_StdUnorderedMap_Insert_String_StdSameAlloc_NoReserve(benchmark::State &state) -> void
  {
    run_insert_string<StdUnorderedMapStringSameAlloc<String, i32, AuStringStdHash, AuStringStdEq>>(state, false);
  }
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Insert_String_StdSameAlloc_NoReserve);

  static auto BM_StdUnorderedMap_Insert_String_StdSameHashAlloc(benchmark::State &state) -> void
  {
    run_insert_string<StdUnorderedMapSameHashAlloc<String, i32, AuStringStdHash, AuStringStdEq>>(state, true);
  }
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Insert_String_StdSameHashAlloc);

  static auto BM_StdUnorderedMap_Insert_String_StdSameHashAlloc_NoReserve(benchmark::State &state) -> void
  {
    run_insert_string<StdUnorderedMapSameHashAlloc<String, i32, AuStringStdHash, AuStringStdEq>>(state, false);
  }
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Insert_String_StdSameHashAlloc_NoReserve);

  // ---------------------------------------------------------------------------
  // Lookup (String)
  // ---------------------------------------------------------------------------

  template<typename Map>
  static auto run_lookup_string(benchmark::State &state, unsigned hit_percent) -> void
  {
    const auto n = static_cast<usize>(state.range(0));
    const auto keys = to_au_strings(make_keys_string_unique(n));
    const auto miss_keys = to_au_strings(make_miss_keys_string_unique(n));
    const auto plan = make_lookup_probe_plan(n, hit_percent, 0xFEEDFACEu);

    Map map;
    configure_std_map(map);
    map.reserve(n);
    for (usize i = 0; i < n; ++i)
    {
      insert_i32(map, keys[i], static_cast<i32>(i));
    }

    run_batched_lookup(
        state, map, keys, miss_keys, plan,
        [](const Map &m, const String &k) -> i64 { return lookup_value_i32(m, k); });
  }

  static auto run_lookup_string_au(benchmark::State &state, unsigned hit_percent) -> void
  {
    const auto n = static_cast<usize>(state.range(0));
    const auto keys = to_au_strings(make_keys_string_unique(n));
    const auto miss_keys = to_au_strings(make_miss_keys_string_unique(n));
    const auto plan = make_lookup_probe_plan(n, hit_percent, 0xFEEDFACEu);

    HashMap<String, i32> map;
    map.reserve(n);
    for (usize i = 0; i < n; ++i)
    {
      map.insert(keys[i], static_cast<i32>(i));
    }

    run_batched_lookup(
        state, map, keys, miss_keys, plan,
        [](const HashMap<String, i32> &m, const String &k) -> i64 {
          const auto *v = m.find(k);
          return v ? static_cast<i64>(*v) : 0;
        });
  }

#define DEFINE_LOOKUP_STRING_BENCH(NAME, HIT)                                                                          \
  static auto BM_AuHashMap_Lookup_String_##NAME(benchmark::State &state) -> void                                     \
  {                                                                                                                    \
    run_lookup_string_au(state, HIT);                                                                                  \
  }                                                                                                                    \
  AU_BENCH_HASH_RANGE(BM_AuHashMap_Lookup_String_##NAME);                                                            \
  static auto BM_StdUnorderedMap_Lookup_String_StdStock_##NAME(benchmark::State &state) -> void                      \
  {                                                                                                                    \
    run_lookup_string<StdUnorderedMapStringStock<String, i32, AuStringStdHash, AuStringStdEq>>(state, HIT);            \
  }                                                                                                                    \
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Lookup_String_StdStock_##NAME);                                               \
  static auto BM_StdUnorderedMap_Lookup_String_StdSameAlloc_##NAME(benchmark::State &state) -> void                  \
  {                                                                                                                    \
    run_lookup_string<StdUnorderedMapStringSameAlloc<String, i32, AuStringStdHash, AuStringStdEq>>(state, HIT);      \
  }                                                                                                                    \
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Lookup_String_StdSameAlloc_##NAME);                                           \
  static auto BM_StdUnorderedMap_Lookup_String_StdSameHashAlloc_##NAME(benchmark::State &state) -> void              \
  {                                                                                                                    \
    run_lookup_string<StdUnorderedMapSameHashAlloc<String, i32, AuStringStdHash, AuStringStdEq>>(state, HIT);       \
  }                                                                                                                    \
  AU_BENCH_HASH_RANGE(BM_StdUnorderedMap_Lookup_String_StdSameHashAlloc_##NAME)

  DEFINE_LOOKUP_STRING_BENCH(Hit100, 100);
  DEFINE_LOOKUP_STRING_BENCH(Hit50, 50);
  DEFINE_LOOKUP_STRING_BENCH(Hit0, 0);

#undef DEFINE_LOOKUP_STRING_BENCH

} // namespace
