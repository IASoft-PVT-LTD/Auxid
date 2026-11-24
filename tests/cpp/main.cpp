// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

#include <auxid/macros.hpp>

import auxid;
import auxid.test;

using namespace au;

int main(int argc, char *argv[])
{
  AU_UNUSED(argc);
  AU_UNUSED(argv);

  struct ThreadInitGuard
  {
    ThreadInitGuard()
    {
      auxid::initialize_main_thread();
    }

    ~ThreadInitGuard()
    {
      auxid::terminate_main_thread();
    }
  } _thread_init_guard;

  au::detail::write_stdout("\033[32m\n================================\n"
                           "   LibAuxid - Unit Test Suite\n"
                           "================================\033[0m\n\n");

  return test::TestRegistry::run_all();
}
