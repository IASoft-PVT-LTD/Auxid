// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

import auxid;
import auxid.test;

using namespace au;

namespace
{
  struct FormatBlock final : test::Block
  {
    [[nodiscard]] auto get_name() const -> const char * override
    {
      return "containers::format";
    }

    auto declare_tests() -> void override
    {
      add_test("std_format_string", [this] { return std_format_string(); });
      add_test("logger_format_string", [this] { return logger_format_string(); });
    }

    auto std_format_string() -> bool
    {
      const String s("hello");
      const StringView sv("world");
      return check_eq(String::format("{}", s), "hello", "String::format with String") &&
             check_eq(String::format("{}", sv), "world", "String::format with StringView");
    }

    auto logger_format_string() -> bool
    {
      const String s("format regression");
      auxid::get_thread_logger().error("{}", s);
      return true;
    }
  };

  const test::AutoRegister<FormatBlock> _registered;
} // namespace
