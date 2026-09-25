// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

#include <algorithm>
#include <iterator>
#include <ranges>

import auxid;
import auxid.test;

using namespace au;

static_assert(std::contiguous_iterator<Vec<i32>::iterator>);
static_assert(std::contiguous_iterator<Vec<i32>::const_iterator>);
static_assert(std::ranges::contiguous_range<Vec<i32>>);

static_assert(std::contiguous_iterator<String::iterator>);
static_assert(std::ranges::contiguous_range<String>);

static_assert(std::contiguous_iterator<StringView::iterator>);
static_assert(std::ranges::contiguous_range<StringView>);
static_assert(std::ranges::borrowed_range<StringView>);

static_assert(std::contiguous_iterator<Span<const i32>::iterator>);
static_assert(std::ranges::contiguous_range<Span<const i32>>);

static_assert(std::contiguous_iterator<HashMap<i32, i32>::iterator>);
static_assert(std::ranges::contiguous_range<HashMap<i32, i32>>);

static_assert(std::contiguous_iterator<HashSet<i32>::iterator>);
static_assert(std::ranges::contiguous_range<HashSet<i32>>);

namespace
{
  struct IteratorConceptsBlock final : test::Block
  {
    [[nodiscard]] auto get_name() const -> const char * override
    {
      return "containers::iterator_concepts";
    }

    auto declare_tests() -> void override
    {
      add_test("std_sort_on_vec", [this] { return std_sort_on_vec(); });
      add_test("std_sort_on_string", [this] { return std_sort_on_string(); });
      add_test("stringview_range_for", [this] { return stringview_range_for(); });
      add_test("hash_map_iteration", [this] { return hash_map_iteration(); });
    }

    auto std_sort_on_vec() -> bool
    {
      Vec<i32> v;
      v.push_back(3);
      v.push_back(1);
      v.push_back(2);
      std::ranges::sort(v);
      return check_eq(v[0], 1, "v[0] == 1") && check_eq(v[1], 2, "v[1] == 2") && check_eq(v[2], 3, "v[2] == 3");
    }

    auto std_sort_on_string() -> bool
    {
      String s = "cba";
      std::ranges::sort(s);
      return check_eq(s, StringView("abc"), "sorted == \"abc\"");
    }

    auto stringview_range_for() -> bool
    {
      StringView sv = "xy";
      usize sum = 0;
      for (char c : sv)
        sum += static_cast<usize>(c);
      return check_eq(sum, static_cast<usize>('x') + static_cast<usize>('y'), "sum == 'x' + 'y'");
    }

    auto hash_map_iteration() -> bool
    {
      HashMap<i32, i32> m;
      m.insert(1, 10);
      m.insert(2, 20);
      usize count = 0;
      for (auto &p : m)
      {
        (void) p;
        ++count;
      }
      return check_eq(count, m.size(), "iteration count == m.size()");
    }
  };

  const test::AutoRegister<IteratorConceptsBlock> _registered;
} // namespace
