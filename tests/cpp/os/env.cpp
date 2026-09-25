// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

// OS-CONTRACT conformance: auxid.env.

#include <auxid/macros.hpp>

#include <string>

import auxid;
import auxid.test;

using namespace au;

namespace
{
  struct EnvBlock final : test::Block
  {
    [[nodiscard]] auto get_name() const -> const char * override
    {
      return "os::env";
    }

    auto declare_tests() -> void override
    {
      add_test("set_find_unset_roundtrip", [this] { return set_find_unset_roundtrip(); });
      add_test("unicode_value_roundtrip", [this] { return unicode_value_roundtrip(); });
      add_test("get_or_fallback", [this] { return get_or_fallback(); });
      add_test("unset_missing_is_ok", [this] { return unset_missing_is_ok(); });
      add_test("executable_path_absolute", [this] { return executable_path_absolute(); });
      add_test("standard_dirs_absolute", [this] { return standard_dirs_absolute(); });
    }

    auto set_find_unset_roundtrip() -> bool
    {
      if (!check(env::set("AUXID_TEST_VAR", "hello-auxid").is_ok(), "set ok"))
        return false;
      auto found = env::find("AUXID_TEST_VAR");
      if (!check(found.has_value(), "find after set"))
        return false;
      if (!check_eq(*found, "hello-auxid", "value round-trips"))
        return false;
      if (!check(env::unset("AUXID_TEST_VAR").is_ok(), "unset ok"))
        return false;
      return check_not(env::find("AUXID_TEST_VAR").has_value(), "gone after unset");
    }

    auto unicode_value_roundtrip() -> bool
    {
      // UTF-8 at the API on every platform (contract): CJK + emoji survive
      // the Windows UTF-16 boundary.
      const char *value = "\xE6\xB8\xAC\xE8\xA9\xA6-\xF0\x9F\x9C\x9A-value";
      if (!check(env::set("AUXID_TEST_UTF8", value).is_ok(), "set utf-8 ok"))
        return false;
      auto found = env::find("AUXID_TEST_UTF8");
      const bool ok = found.has_value() && *found == value;
      (void) env::unset("AUXID_TEST_UTF8");
      return check(ok, "utf-8 value round-trips exactly");
    }

    auto get_or_fallback() -> bool
    {
      return check_eq(env::get_or("AUXID_TEST_DEFINITELY_MISSING", "fallback"), "fallback",
                      "get_or returns fallback for missing");
    }

    auto unset_missing_is_ok() -> bool
    {
      return check(env::unset("AUXID_TEST_DEFINITELY_MISSING").is_ok(),
                   "unsetting a missing variable succeeds (contract)");
    }

    auto executable_path_absolute() -> bool
    {
      auto exe = env::executable_path();
#if defined(AU_PLATFORM_WASM)
      // Honest absence (contract): no executable image in a wasm sandbox.
      return check(exe.is_err(), "executable_path is an honest Err on wasm");
#else
      if (!check(exe.is_ok(), "executable_path ok"))
        return false;
      return check(exe.unwrap().is_absolute(), "executable_path is absolute");
#endif
    }

    auto standard_dirs_absolute() -> bool
    {
      const env::StandardDir dirs[] = {env::StandardDir::Config, env::StandardDir::Cache,
                                       env::StandardDir::Data,   env::StandardDir::Runtime,
                                       env::StandardDir::Home,   env::StandardDir::Temp};
      for (const auto dir : dirs)
      {
        auto path = env::standard_dir(dir, "AuxidTest");
        if (!check(path.is_ok(), "standard_dir resolves"))
          return false;
        if (!check(path.unwrap().is_absolute(), "standard_dir is absolute"))
          return false;
      }

      auto config = env::standard_dir(env::StandardDir::Config, "AuxidTest");
      const auto config_str = config.unwrap().generic_string();
      return check(config_str.find("AuxidTest") != std::string::npos,
                   "app-scoped dir contains the app name");
    }
  };

  const test::AutoRegister<EnvBlock> _registered;
} // namespace
