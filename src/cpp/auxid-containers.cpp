// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

module;

#include <auxid/macros.hpp>
#include <auxid_win32.hpp>

#include <atomic>
#include <cerrno>
#include <random>

module auxid.containers;

namespace au
{
  auto Error::os_last() -> Error
  {
#if AU_PLATFORM_WINDOWS
    return Error(ErrorDomain::Os, static_cast<i32>(::GetLastError()));
#else
    return Error(ErrorDomain::Os, errno);
#endif
  }
} // namespace au

namespace au::containers::detail
{
  [[nodiscard]] auto random_seed_64() noexcept -> u64
  {
    static std::atomic<u64> g_salt{0x9E3779B97F4A7C15ULL};

    struct ThreadRng
    {
      std::mt19937_64 engine;

      ThreadRng() noexcept
      {
        std::random_device rd;
        const u64 a = (static_cast<u64>(rd()) << 32) ^ static_cast<u64>(rd());
        const u64 b = (static_cast<u64>(rd()) << 32) ^ static_cast<u64>(rd());
        const u64 salt = g_salt.fetch_add(0xBF58476D1CE4E5B9ULL, std::memory_order_relaxed);
        engine.seed(wymix(a ^ salt, b ^ 0x94D049BB133111EBULL));
      }
    };

    thread_local ThreadRng rng;
    return rng.engine();
  }
} // namespace au::containers::detail
