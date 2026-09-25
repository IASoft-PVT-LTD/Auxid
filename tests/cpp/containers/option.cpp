// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

#include <optional>
#include <utility>

import auxid;
import auxid.test;

using namespace au;

namespace
{
  struct OptionBlock final : test::Block
  {
    [[nodiscard]] auto get_name() const -> const char * override
    {
      return "containers::option";
    }

    auto declare_tests() -> void override
    {
      add_test("some_and_none", [this] { return some_and_none(); });
      add_test("complex_type", [this] { return complex_type(); });
      add_test("std_interop_some", [this] { return std_interop_some(); });
      add_test("std_interop_none", [this] { return std_interop_none(); });
      add_test("std_interop_move", [this] { return std_interop_move(); });
    }

    auto some_and_none() -> bool
    {
      Option<i32> opt1 = 42;
      if (!check(opt1.is_some(), "opt1.is_some()"))
        return false;
      if (!check_not(opt1.is_none(), "!opt1.is_none()"))
        return false;
      if (!check_eq(opt1.unwrap(), 42, "opt1.unwrap() == 42"))
        return false;

      Option<i32> opt2 = containers::nullopt;
      return check(opt2.is_none(), "opt2.is_none()") && check_not(opt2.is_some(), "!opt2.is_some()") &&
             check_eq(opt2.value_or(10), 10, "opt2.value_or(10) == 10");
    }

    auto complex_type() -> bool
    {
      Option<String> str_opt = String("Test");
      if (!check(str_opt.has_value(), "str_opt.has_value()"))
        return false;
      if (!check_eq(str_opt.unwrap(), "Test", "str_opt.unwrap() == \"Test\""))
        return false;

      str_opt = containers::nullopt;
      return check(str_opt.is_none(), "str_opt.is_none() after reset");
    }

    auto std_interop_some() -> bool
    {
      std::optional<i32> from_std{42};
      Option<i32> ours = from_std;
      if (!check(ours.is_some(), "ours.is_some()"))
        return false;
      if (!check_eq(ours.unwrap(), 42, "ours.unwrap() == 42"))
        return false;

      std::optional<i32> back_to_std = ours;
      return check(back_to_std.has_value(), "back_to_std.has_value()") &&
             check_eq(*back_to_std, 42, "*back_to_std == 42");
    }

    auto std_interop_none() -> bool
    {
      std::optional<i32> empty;
      Option<i32> ours = empty;
      if (!check(ours.is_none(), "ours.is_none()"))
        return false;

      std::optional<i32> back_to_std = ours;
      return check_not(back_to_std.has_value(), "!back_to_std.has_value()");
    }

    auto std_interop_move() -> bool
    {
      std::optional<String> from_std{String("hello")};
      Option<String> ours = std::move(from_std);
      return check(ours.is_some(), "ours.is_some()") && check_eq(ours.unwrap(), "hello", "ours.unwrap() == \"hello\"");
    }
  };

  const test::AutoRegister<OptionBlock> _registered;
} // namespace
