// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

#include <cstdint>
#include <stdexcept>

#include <benchmark/benchmark.h>

#include "exception_baseline_shared.hpp"
#include "result_vs_exceptions_shared.hpp"

namespace
{
  struct FailMessageVerifier
  {
    FailMessageVerifier()
    {
      au_bench::verify_fail_message_lengths();
    }
  };

  const FailMessageVerifier g_fail_message_verifier{};

  auto run_throw(benchmark::State &state, std::uint32_t fail_threshold) -> void
  {
    const auto workload_size = static_cast<std::size_t>(state.range(0));
    const auto depth = static_cast<std::uint32_t>(state.range(1));

    for (auto _ : state)
    {
      au_bench::Lcg rng{au_bench::LCG_SEED};
      std::int64_t acc = 0;
      std::int64_t err_count = 0;

      for (std::size_t i = 0; i < workload_size; ++i)
      {
        const auto r = rng.next();
        const bool should_fail = r < fail_threshold;
        try
        {
          acc += au_bench::chain_compute_throw(depth, static_cast<std::int32_t>(i), should_fail);
        }
        catch (const std::runtime_error &)
        {
          ++err_count;
        }
      }
      benchmark::DoNotOptimize(acc);
      benchmark::DoNotOptimize(err_count);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(workload_size) *
                            static_cast<std::int64_t>(depth));
  }
} // namespace

static auto BM_Exception_HappyPath(benchmark::State &state) -> void
{
  run_throw(state, au_bench::THRESHOLD_0_PCT);
}
AU_BENCH_RESULT_ARGS(BM_Exception_HappyPath);

static auto BM_Exception_ErrorRate_1pct(benchmark::State &state) -> void
{
  run_throw(state, au_bench::THRESHOLD_1_PCT);
}
AU_BENCH_RESULT_ARGS(BM_Exception_ErrorRate_1pct);

static auto BM_Exception_ErrorRate_5pct(benchmark::State &state) -> void
{
  run_throw(state, au_bench::THRESHOLD_5_PCT);
}
AU_BENCH_RESULT_ARGS(BM_Exception_ErrorRate_5pct);

static auto BM_Exception_ErrorRate_25pct(benchmark::State &state) -> void
{
  run_throw(state, au_bench::THRESHOLD_25_PCT);
}
AU_BENCH_RESULT_ARGS(BM_Exception_ErrorRate_25pct);

static auto BM_Exception_ErrorRate_50pct(benchmark::State &state) -> void
{
  run_throw(state, au_bench::THRESHOLD_50_PCT);
}
AU_BENCH_RESULT_ARGS(BM_Exception_ErrorRate_50pct);
