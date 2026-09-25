// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

// OS-CONTRACT conformance: auxid.dl, against a system library present on
// every CI runner.

#include <auxid/macros.hpp>

import auxid;
import auxid.test;

using namespace au;

namespace
{
  struct DlBlock final : test::Block
  {
    [[nodiscard]] auto get_name() const -> const char * override
    {
      return "os::dl";
    }

    auto declare_tests() -> void override
    {
#if defined(AU_PLATFORM_WASM)
      add_test("honest_absence", [this] { return honest_absence(); });
#else
      add_test("open_symbol_close", [this] { return open_symbol_close(); });
      add_test("missing_symbol_is_err", [this] { return missing_symbol_is_err(); });
#endif
    }

#if defined(AU_PLATFORM_WASM)

    auto honest_absence() -> bool
    {
      return check(dl::Library::open(filesystem::Path("libc.so")).is_err(),
                   "open is an honest Err on wasm");
    }

#else

    [[nodiscard]] static auto system_lib() -> filesystem::Path
    {
#  if AU_PLATFORM_WINDOWS
      return filesystem::Path("kernel32.dll");
#  elif defined(AU_PLATFORM_APPLE)
      return filesystem::Path("/usr/lib/libSystem.B.dylib");
#  else
      return filesystem::Path("libm.so.6");
#  endif
    }

    [[nodiscard]] static auto known_symbol() -> const char *
    {
#  if AU_PLATFORM_WINDOWS
      return "GetTickCount";
#  else
      return "cos";
#  endif
    }

    auto open_symbol_close() -> bool
    {
      auto lib_res = dl::Library::open(system_lib());
      if (!check(lib_res.is_ok(), "system library opens"))
        return false;
      auto lib = static_cast<dl::Library &&>(lib_res.unwrap());

      auto sym = lib.symbol(known_symbol());
      if (!check(sym.is_ok(), "known symbol resolves"))
        return false;
      if (!check(sym.unwrap() != nullptr, "symbol address is non-null"))
        return false;

      return check(lib.close().is_ok(), "close reports Ok");
    }

    auto missing_symbol_is_err() -> bool
    {
      auto lib_res = dl::Library::open(system_lib());
      if (!check(lib_res.is_ok(), "system library opens"))
        return false;
      auto lib = static_cast<dl::Library &&>(lib_res.unwrap());
      return check(lib.symbol("auxid_definitely_not_a_symbol").is_err(),
                   "missing symbol is an Err");
    }

#endif
  };

  const test::AutoRegister<DlBlock> _registered;
} // namespace
