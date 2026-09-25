// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

#pragma once

#include <cstddef>
#include <cstdint>

namespace au_bench
{
  inline constexpr const char *FAIL_MESSAGE = "compute_failed";
  inline constexpr std::size_t FAIL_MESSAGE_LEN = sizeof("compute_failed") - 1;

  static_assert(FAIL_MESSAGE_LEN == 14, "Result and exception payloads must stay identical length");

  inline constexpr std::uint64_t LCG_SEED = 0xDEC0DECULL;

  struct Lcg
  {
    std::uint64_t state;

    constexpr explicit Lcg(std::uint64_t seed) noexcept : state(seed)
    {
    }

    constexpr auto next() noexcept -> std::uint32_t
    {
      state = state * 6364136223846793005ULL + 1442695040888963407ULL;
      return static_cast<std::uint32_t>(state >> 32);
    }
  };

  inline constexpr std::uint32_t THRESHOLD_0_PCT = 0u;
  inline constexpr std::uint32_t THRESHOLD_1_PCT =
      static_cast<std::uint32_t>(0.01 * static_cast<double>(UINT32_MAX));
  inline constexpr std::uint32_t THRESHOLD_5_PCT =
      static_cast<std::uint32_t>(0.05 * static_cast<double>(UINT32_MAX));
  inline constexpr std::uint32_t THRESHOLD_25_PCT =
      static_cast<std::uint32_t>(0.25 * static_cast<double>(UINT32_MAX));
  inline constexpr std::uint32_t THRESHOLD_50_PCT =
      static_cast<std::uint32_t>(0.50 * static_cast<double>(UINT32_MAX));

#define AU_BENCH_RESULT_ARGS(BM)                                                                                       \
  BENCHMARK(BM)                                                                                                        \
      ->Args({1024, 1})                                                                                                \
      ->Args({1024, 4})                                                                                                \
      ->Args({1024, 8})                                                                                                \
      ->Args({16384, 1})                                                                                               \
      ->Args({16384, 4})                                                                                               \
      ->Args({16384, 8})

} // namespace au_bench
