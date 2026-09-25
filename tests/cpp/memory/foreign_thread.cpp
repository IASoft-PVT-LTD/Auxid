// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

// Deliberately a raw std::thread: it simulates a FOREIGN thread — one no Auxid
// thread guard ever touched (a web-engine callback, an OS thread-pool thread).
// au::Thread would install a WorkerThreadGuard and defeat the test's purpose.
#include <thread>

import auxid;
import auxid.test;

using namespace au;

namespace
{
  // Regression test for the foreign-thread allocation guarantee (D-006):
  // HeapAllocator must be safe on threads with no guard — rpmalloc lazily
  // creates the thread heap on first allocation — and blocks allocated on a
  // foreign thread must be freeable from another thread.
  struct ForeignThreadBlock final : test::Block
  {
    [[nodiscard]] auto get_name() const -> const char * override
    {
      return "memory::foreign_thread";
    }

    auto declare_tests() -> void override
    {
      add_test("lazy_thread_heap", [this] { return lazy_thread_heap(); });
    }

    auto lazy_thread_heap() -> bool
    {
      String cross_thread;
      bool allocated_ok = false;

      std::thread raw([&cross_thread, &allocated_ok] {
        // No WorkerThreadGuard on purpose.
        String local("built on a foreign thread with no Auxid thread guard installed;");
        local.append(" long enough to defeat SSO and force a real heap allocation");

        HashMap<i32, i32> map;
        for (i32 i = 0; i < 256; i++)
          map.insert(i, i * 2);

        const i32 *probe = map.find(128);
        allocated_ok = local.size() > String::SSO_CAPACITY && probe != nullptr && *probe == 256;

        // Handing the heap block to the main thread exercises the
        // cross-thread free path when cross_thread dies below.
        cross_thread = std::move(local);
      });
      raw.join();

      if (!check(allocated_ok, "foreign thread allocated and read back without a guard"))
        return false;
      return check(cross_thread.find("foreign") != String::npos,
                   "foreign-thread block readable and freeable from the main thread");
    }
  };

  const test::AutoRegister<ForeignThreadBlock> _registered;
} // namespace
