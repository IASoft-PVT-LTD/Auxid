// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

#include <auxid/macros.hpp>
#include <benchmark/benchmark.h>

#include "result_vs_exceptions_shared.hpp"

import auxid;

using namespace au;

namespace
{
#if defined(_MSC_VER)
  __declspec(noinline)
#else
  [[gnu::noinline]]
#endif
  static auto compute_result(i32 in, bool fail_this_call) -> Result<i32>
  {
    if AU_UNLIKELY (fail_this_call)
    {
      return fail(String{au_bench::FAIL_MESSAGE});
    }
    return in * 3 + 7;
  }

#if defined(_MSC_VER)
  __declspec(noinline)
#else
  [[gnu::noinline]]
#endif
  static auto chain_compute_result(u32 depth, i32 in, bool fail_this_call) -> Result<i32>
  {
    if (depth <= 1)
    {
      return compute_result(in, fail_this_call);
    }
    const auto inner = chain_compute_result(depth - 1, in, fail_this_call);
    if (!inner.is_ok())
    {
      return inner;
    }
    return inner.unwrap();
  }

  auto run_result(benchmark::State &state, u32 fail_threshold) -> void
  {
    const auto workload_size = static_cast<usize>(state.range(0));
    const auto depth = static_cast<u32>(state.range(1));

    for (auto _ : state)
    {
      au_bench::Lcg rng{au_bench::LCG_SEED};
      i64 acc = 0;
      i64 err_count = 0;

      for (usize i = 0; i < workload_size; ++i)
      {
        const auto r = rng.next();
        const bool should_fail = r < fail_threshold;
        Result<i32> out = chain_compute_result(depth, static_cast<i32>(i), should_fail);
        if (out.is_ok())
        {
          acc += out.unwrap();
        }
        else
        {
          ++err_count;
        }
      }
      benchmark::DoNotOptimize(acc);
      benchmark::DoNotOptimize(err_count);
    }
    state.SetItemsProcessed(static_cast<i64>(state.iterations()) * static_cast<i64>(workload_size) *
                            static_cast<i64>(depth));
  }
} // namespace

static auto BM_AuResult_HappyPath(benchmark::State &state) -> void
{
  run_result(state, au_bench::THRESHOLD_0_PCT);
}
AU_BENCH_RESULT_ARGS(BM_AuResult_HappyPath);

static auto BM_AuResult_ErrorRate_1pct(benchmark::State &state) -> void
{
  run_result(state, au_bench::THRESHOLD_1_PCT);
}
AU_BENCH_RESULT_ARGS(BM_AuResult_ErrorRate_1pct);

static auto BM_AuResult_ErrorRate_5pct(benchmark::State &state) -> void
{
  run_result(state, au_bench::THRESHOLD_5_PCT);
}
AU_BENCH_RESULT_ARGS(BM_AuResult_ErrorRate_5pct);

static auto BM_AuResult_ErrorRate_25pct(benchmark::State &state) -> void
{
  run_result(state, au_bench::THRESHOLD_25_PCT);
}
AU_BENCH_RESULT_ARGS(BM_AuResult_ErrorRate_25pct);

static auto BM_AuResult_ErrorRate_50pct(benchmark::State &state) -> void
{
  run_result(state, au_bench::THRESHOLD_50_PCT);
}
AU_BENCH_RESULT_ARGS(BM_AuResult_ErrorRate_50pct);
