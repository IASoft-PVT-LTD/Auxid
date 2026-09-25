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

// Textual <filesystem>: Path codegen in a module impl unit (Homebrew LLVM
// 22.1.8 hazard, see auxid-env.cpp).
#include <filesystem>
#include <string_view>
#include <utility>

#if !AU_PLATFORM_WINDOWS && !defined(AU_PLATFORM_WASM)
#  include <dlfcn.h>
#endif

module auxid.dl;

import auxid.memory;
import auxid.containers;

namespace au::dl
{
#if defined(AU_PLATFORM_WASM)

  AUXID_API auto Library::open(const filesystem::Path &) -> Result<Library>
  {
    // Honest absence: emscripten dlopen exists only under MAIN_MODULE
    // builds, which Auxid does not target in v1.
    return fail("dl::Library::open: not expressible in a wasm sandbox");
  }

  AUXID_API auto Library::symbol(StringView) -> Result<void *>
  {
    return fail("dl::Library::symbol: not expressible in a wasm sandbox");
  }

  AUXID_API auto Library::close() -> Result<void>
  {
    m_native = nullptr;
    return {};
  }

#elif AU_PLATFORM_WINDOWS

  AUXID_API auto Library::open(const filesystem::Path &path) -> Result<Library>
  {
    void *module_handle = ::LoadLibraryW(path.c_str());
    if (module_handle == nullptr)
      return fail(Error::os_last().ctx("dl::Library::open"));
    Library lib;
    lib.m_native = module_handle;
    return lib;
  }

  AUXID_API auto Library::symbol(StringView name) -> Result<void *>
  {
    if (m_native == nullptr)
      return fail("dl::Library::symbol: library is not open");
    const String zname(name);
    void *address = ::GetProcAddress(m_native, zname.c_str());
    if (address == nullptr)
      return fail(Error::os_last().ctx("dl::Library::symbol"));
    return address;
  }

  AUXID_API auto Library::close() -> Result<void>
  {
    if (m_native == nullptr)
      return {};
    void *module_handle = m_native;
    m_native = nullptr;
    if (::FreeLibrary(module_handle) == 0)
      return fail(Error::os_last().ctx("dl::Library::close"));
    return {};
  }

#else // POSIX

  AUXID_API auto Library::open(const filesystem::Path &path) -> Result<Library>
  {
    void *handle = ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr)
    {
      const char *reason = ::dlerror();
      return fail("dl::Library::open: {}", reason != nullptr ? reason : "unknown dlopen failure");
    }
    Library lib;
    lib.m_native = handle;
    return lib;
  }

  AUXID_API auto Library::symbol(StringView name) -> Result<void *>
  {
    if (m_native == nullptr)
      return fail("dl::Library::symbol: library is not open");
    const String zname(name);
    ::dlerror(); // clear any stale error: null symbol values are legal
    void *address = ::dlsym(m_native, zname.c_str());
    const char *reason = ::dlerror();
    if (reason != nullptr)
      return fail("dl::Library::symbol: {}", reason);
    return address;
  }

  AUXID_API auto Library::close() -> Result<void>
  {
    if (m_native == nullptr)
      return {};
    void *handle = m_native;
    m_native = nullptr;
    if (::dlclose(handle) != 0)
    {
      const char *reason = ::dlerror();
      return fail("dl::Library::close: {}", reason != nullptr ? reason : "unknown dlclose failure");
    }
    return {};
  }

#endif
} // namespace au::dl
