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

#include <print>
#include <string_view>
#include <vector>

module auxid.test;

namespace au::test
{
  AUXID_API auto TestRegistry::get_entries() -> Vec<TestEntry> &
  {
    static Vec<TestEntry> entries;
    return entries;
  }

  AUXID_API auto TestRegistry::run_all() -> i32
  {
    DefaultRunner r;
    Vec<TestEntry> &entries = get_entries();
    impl::print_discovered(entries.size());

    for (usize i = 0; i < entries.size(); ++i)
    {
      entries[i](r);
    }

    return r.fail_count() == 0 ? 0 : 1;
  }

  AUXID_API auto register_test_block(TestRegistry::TestEntry entry) -> void
  {
    TestRegistry::get_entries().push_back(entry);
  }
} // namespace au::test

namespace au::test::impl
{
  AUXID_API auto print_block_header(std::string_view name) -> void
  {
    std::println("{}Testing [{}]...{}", console::MAGENTA, name, console::RESET);
  }

  AUXID_API auto print_test_progress(std::string_view name) -> void
  {
    std::print("{}  Testing {}...\n{}", console::YELLOW, name, console::RESET);
  }

  AUXID_API auto print_blank_line() -> void
  {
    // Not std::println(): the zero-argument overload is a late C++23 addition
    // (P3142) that libc++ 20 does not ship yet.
    std::print("\n");
  }

  AUXID_API auto print_discovered(usize count) -> void
  {
    std::print("{}[AUTest] Discovered {} Test Blocks\n\n{}", console::CYAN, count, console::RESET);
  }

  AUXID_API auto print_summary(usize fail_count, usize test_count, usize block_count) -> void
  {
    std::println("{}\n-----------------------------------\n\t      SUMMARY\n-----------------------------------",
                 console::GREEN);

    if (fail_count == 0)
    {
      std::println("\n\tALL TESTS PASSED!\n");
    }
    else
    {
      const f64 success_rate =
          test_count == 0 ? 0.0 : (100.0 * static_cast<f64>(test_count - fail_count) / static_cast<f64>(test_count));
      std::println("{}{} OF {} TESTS FAILED\n{}Success Rate: {:.2f}%", console::RED, fail_count, test_count,
                   console::YELLOW, success_rate);
    }

    std::println("{}Ran {} test(s) across {} block(s)\n{}-----------------------------------{}", console::MAGENTA,
                 test_count, block_count, console::GREEN, console::RESET);
  }
} // namespace au::test::impl
