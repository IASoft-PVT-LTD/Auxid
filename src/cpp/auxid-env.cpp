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

#include <cerrno>
#include <cstdlib>
// Textual <filesystem>: this TU does Path codegen; relying only on the
// module-imported declarations ICEs Homebrew LLVM 22.1.8 on macOS
// (crash emitting std::__libcpp_allocate for module-reachable decls).
#include <filesystem>
#include <string_view>
#include <utility>

#if !AU_PLATFORM_WINDOWS
#  include <unistd.h>
#  if defined(AU_PLATFORM_APPLE)
#    include <mach-o/dyld.h>
#  endif
#endif

module auxid.env;

import auxid.memory;
import auxid.containers;

namespace au::env
{
  namespace
  {
    [[nodiscard]] auto path_from_utf8(StringView sv) -> filesystem::Path
    {
#if AU_PLATFORM_WINDOWS
      // char8_t source: converts UTF-8 to the native wide encoding.
      return filesystem::Path(
          std::u8string_view(reinterpret_cast<const char8_t *>(sv.data()), sv.size()));
#else
      // POSIX paths ARE bytes; plain chars pass through untranslated. (Also
      // sidesteps a Homebrew LLVM 22.1.8 codegen crash in the char8_t
      // conversion machinery observed on macOS CI.)
      return filesystem::Path(std::string_view(sv.data(), sv.size()));
#endif
    }

#if AU_PLATFORM_WINDOWS

    [[nodiscard]] auto utf8_to_wide(StringView utf8) -> Vec<wchar_t>
    {
      Vec<wchar_t> wide;
      if (!utf8.empty())
      {
        const int needed = ::MultiByteToWideChar(win32::CP_UTF8_, 0, utf8.data(),
                                                 static_cast<int>(utf8.size()), nullptr, 0);
        if (needed > 0)
        {
          wide.resize(static_cast<usize>(needed));
          ::MultiByteToWideChar(win32::CP_UTF8_, 0, utf8.data(), static_cast<int>(utf8.size()),
                                wide.data(), needed);
        }
      }
      wide.push_back(L'\0');
      return wide;
    }

    [[nodiscard]] auto wide_to_utf8(const wchar_t *wide, usize wide_len) -> String
    {
      if (wide_len == 0)
        return String();
      const int needed = ::WideCharToMultiByte(win32::CP_UTF8_, 0, wide, static_cast<int>(wide_len),
                                               nullptr, 0, nullptr, nullptr);
      if (needed <= 0)
        return String();
      Vec<char> utf8(static_cast<usize>(needed));
      ::WideCharToMultiByte(win32::CP_UTF8_, 0, wide, static_cast<int>(wide_len), utf8.data(), needed,
                            nullptr, nullptr);
      return String(utf8.data(), utf8.size());
    }

#endif
  } // namespace

#if AU_PLATFORM_WINDOWS

  AUXID_API auto find(StringView name) -> Option<String>
  {
    const Vec<wchar_t> wname = utf8_to_wide(name);

    unsigned long size = ::GetEnvironmentVariableW(wname.data(), nullptr, 0);
    if (size == 0)
      return {}; // includes ERROR_ENVVAR_NOT_FOUND

    // size includes the terminator on the query call. Loop in case the value
    // changes between the two calls.
    for (int attempt = 0; attempt < 4; attempt++)
    {
      Vec<wchar_t> buffer(static_cast<usize>(size));
      const unsigned long written = ::GetEnvironmentVariableW(wname.data(), buffer.data(), size);
      if (written == 0)
        return {};
      if (written < size)
        return wide_to_utf8(buffer.data(), static_cast<usize>(written));
      size = written + 1;
    }
    return {};
  }

  AUXID_API auto set(StringView name, StringView value) -> Result<void>
  {
    const Vec<wchar_t> wname = utf8_to_wide(name);
    const Vec<wchar_t> wvalue = utf8_to_wide(value);
    if (::SetEnvironmentVariableW(wname.data(), wvalue.data()) == 0)
      return fail(std::move(Error::os_last()).ctx("env::set"));
    return {};
  }

  AUXID_API auto unset(StringView name) -> Result<void>
  {
    const Vec<wchar_t> wname = utf8_to_wide(name);
    if (::SetEnvironmentVariableW(wname.data(), nullptr) == 0)
    {
      Error err = Error::os_last();
      if (static_cast<unsigned long>(err.code) == win32::ERROR_ENVVAR_NOT_FOUND_)
        return {}; // unsetting a missing variable is success (contract)
      return fail(std::move(err).ctx("env::unset"));
    }
    return {};
  }

  AUXID_API auto executable_path() -> Result<filesystem::Path>
  {
    Vec<wchar_t> buffer(512);
    for (int attempt = 0; attempt < 8; attempt++)
    {
      const unsigned long written =
          ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<unsigned long>(buffer.size()));
      if (written == 0)
        return fail(std::move(Error::os_last()).ctx("env::executable_path"));
      if (static_cast<usize>(written) < buffer.size())
        return filesystem::Path(buffer.data(), buffer.data() + written);
      buffer.resize(buffer.size() * 2);
    }
    return fail("env::executable_path: path length is unreasonable");
  }

#else

  AUXID_API auto find(StringView name) -> Option<String>
  {
    const String zname(name);
    const char *value = ::getenv(zname.c_str());
    if (value == nullptr)
      return {};
    return String(value);
  }

  AUXID_API auto set(StringView name, StringView value) -> Result<void>
  {
    const String zname(name);
    const String zvalue(value);
    if (::setenv(zname.c_str(), zvalue.c_str(), 1) != 0)
      return fail(Error::os(errno).ctx("env::set"));
    return {};
  }

  AUXID_API auto unset(StringView name) -> Result<void>
  {
    const String zname(name);
    if (::unsetenv(zname.c_str()) != 0)
      return fail(Error::os(errno).ctx("env::unset"));
    return {};
  }

  AUXID_API auto executable_path() -> Result<filesystem::Path>
  {
#  if defined(AU_PLATFORM_WASM)
    // Honest absence (contract): a wasm sandbox has no executable image path.
    return fail("executable_path: not expressible in a wasm sandbox");
#  elif defined(AU_PLATFORM_APPLE)
    u32 size = 0;
    ::_NSGetExecutablePath(nullptr, &size); // reports required size
    Vec<char> buffer(static_cast<usize>(size) + 1);
    if (::_NSGetExecutablePath(buffer.data(), &size) != 0)
      return fail("env::executable_path: _NSGetExecutablePath failed after sizing");
    return path_from_utf8(StringView(buffer.data(), compiler::strlen(buffer.data())));
#  else
    Vec<char> buffer(1024);
    for (int attempt = 0; attempt < 8; attempt++)
    {
      const isize written = ::readlink("/proc/self/exe", buffer.data(), buffer.size());
      if (written < 0)
        return fail(Error::os(errno).ctx("env::executable_path"));
      if (static_cast<usize>(written) < buffer.size())
        return path_from_utf8(StringView(buffer.data(), static_cast<usize>(written)));
      buffer.resize(buffer.size() * 2);
    }
    return fail("env::executable_path: path length is unreasonable");
#  endif
  }

#endif

  AUXID_API auto get_or(StringView name, StringView fallback) -> String
  {
    auto found = find(name);
    if (found.has_value())
      return std::move(*found);
    return String(fallback);
  }

  namespace
  {
    [[nodiscard]] auto required_var(const char *name) -> Result<String>
    {
      auto found = find(name);
      if (!found.has_value())
        return fail("standard_dir: required environment variable {} is not set", name);
      return std::move(*found);
    }

    // Paths are assembled as au::String and converted to Path exactly once
    // at the return: keeps libc++ path instantiations (operator/, copies)
    // out of this TU — Homebrew LLVM 22.1.8 ICEs on heavy path codegen here.
    [[nodiscard]] auto joined(String base, StringView leaf) -> String
    {
      base.append("/");
      base.append(leaf);
      return base;
    }

    [[nodiscard]] auto temp_base() -> Result<String>
    {
#if AU_PLATFORM_WINDOWS
      auto temp = find("TEMP");
      if (!temp.has_value())
        temp = find("TMP");
      if (!temp.has_value())
        return fail("standard_dir: neither TEMP nor TMP is set");
      return std::move(*temp);
#else
      auto temp = find("TMPDIR");
      if (temp.has_value())
        return std::move(*temp);
      return String("/tmp");
#endif
    }

    [[nodiscard]] auto var_or_home_suffix(const char *var, const char *home_suffix) -> Result<String>
    {
      auto explicit_dir = find(var);
      if (explicit_dir.has_value())
        return std::move(*explicit_dir);
      AU_TRY_VAR(home, required_var("HOME"));
      return joined(std::move(home), home_suffix);
    }
  } // namespace

  // The per-OS mapping table is normative in docs/OS-CONTRACT.md; this is it.
  AUXID_API auto standard_dir(StandardDir dir, StringView app_name) -> Result<filesystem::Path>
  {
    auto build_dir = [&](Result<String> base_res) -> Result<String> {
      AU_TRY_VAR(base, std::move(base_res));
      return joined(std::move(base), app_name);
    };

    Result<String> resolved = fail("standard_dir: unknown StandardDir value");

#if AU_PLATFORM_WINDOWS
    switch (dir)
    {
    case StandardDir::Home:
      resolved = required_var("USERPROFILE");
      break;
    case StandardDir::Temp:
      resolved = temp_base();
      break;
    case StandardDir::Config:
      resolved = build_dir(required_var("APPDATA"));
      break;
    case StandardDir::Data:
      resolved = build_dir(required_var("LOCALAPPDATA"));
      break;
    case StandardDir::Cache: {
      auto base = build_dir(required_var("LOCALAPPDATA"));
      if (base.is_ok())
        resolved = joined(std::move(base.unwrap()), "cache");
      else
        resolved = std::move(base);
      break;
    }
    case StandardDir::Runtime: {
      auto base = build_dir(required_var("LOCALAPPDATA"));
      if (base.is_ok())
        resolved = joined(std::move(base.unwrap()), "runtime");
      else
        resolved = std::move(base);
      break;
    }
    }
#elif defined(AU_PLATFORM_APPLE)
    switch (dir)
    {
    case StandardDir::Home:
      resolved = required_var("HOME");
      break;
    case StandardDir::Temp:
      resolved = temp_base();
      break;
    case StandardDir::Config:
    case StandardDir::Data: {
      auto home = required_var("HOME");
      if (home.is_ok())
        resolved = joined(joined(std::move(home.unwrap()), "Library/Application Support"),
                          app_name);
      else
        resolved = std::move(home);
      break;
    }
    case StandardDir::Cache: {
      auto home = required_var("HOME");
      if (home.is_ok())
        resolved = joined(joined(std::move(home.unwrap()), "Library/Caches"), app_name);
      else
        resolved = std::move(home);
      break;
    }
    case StandardDir::Runtime:
      resolved = build_dir(temp_base());
      break;
    }
#else
    switch (dir)
    {
    case StandardDir::Home:
      resolved = required_var("HOME");
      break;
    case StandardDir::Temp:
      resolved = temp_base();
      break;
    case StandardDir::Config:
      resolved = build_dir(var_or_home_suffix("XDG_CONFIG_HOME", ".config"));
      break;
    case StandardDir::Data:
      resolved = build_dir(var_or_home_suffix("XDG_DATA_HOME", ".local/share"));
      break;
    case StandardDir::Cache:
      resolved = build_dir(var_or_home_suffix("XDG_CACHE_HOME", ".cache"));
      break;
    case StandardDir::Runtime: {
      auto runtime = find("XDG_RUNTIME_DIR");
      if (runtime.has_value())
        resolved = joined(std::move(*runtime), app_name);
      else
        resolved = build_dir(temp_base()); // honest fallback, documented in the contract
      break;
    }
    }
#endif

    AU_TRY_VAR(final_path, std::move(resolved));
    return path_from_utf8(StringView(final_path.data(), final_path.size()));
  }
} // namespace au::env
