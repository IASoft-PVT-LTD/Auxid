// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

// OS Foundation Layer: runtime environment (docs/OS-CONTRACT.md). Strings
// are UTF-8 at this API on every platform; UTF-16 conversion happens inside
// the Windows implementation. Compile-time target facts live in au::build
// (auxid.core) — this module is the RUNTIME environment (D-010 ASK-6).
//
// Thread-safety note (contract): the process environment is global mutable
// state; concurrent set/unset against find from other threads is unsafe on
// POSIX (getenv/setenv contract). Mutate the environment before spawning
// threads, or serialize access at the application level.

module;

#include <auxid/macros.hpp>

export module auxid.env;

export import auxid.core;
export import auxid.fs;
import auxid.containers;

export namespace au::env
{
  // Missing variable -> nullopt. Present-but-empty -> Some(""). (POSIX
  // cannot always distinguish these two on every libc; the contract
  // documents the honest answer per platform.)
  AUXID_API auto find(StringView name) -> Option<String>;

  AUXID_API auto get_or(StringView name, StringView fallback) -> String;

  AUXID_API auto set(StringView name, StringView value) -> Result<void>;
  AUXID_API auto unset(StringView name) -> Result<void>;

  // Absolute path of the running executable image.
  AUXID_API auto executable_path() -> Result<filesystem::Path>;

  // Conventional per-user directories. Returns the conventional ABSOLUTE
  // path for this platform and app; does NOT create the directory. The
  // per-OS mapping table lives in docs/OS-CONTRACT.md. app_name must be a
  // filesystem-safe single component (not validated in v1).
  enum class StandardDir : u8
  {
    Config,
    Cache,
    Data,
    Runtime,
    Home,
    Temp,
  };

  AUXID_API auto standard_dir(StandardDir dir, StringView app_name) -> Result<filesystem::Path>;
} // namespace au::env
