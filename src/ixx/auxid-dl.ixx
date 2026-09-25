// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

// OS Foundation Layer: dynamic libraries (docs/OS-CONTRACT.md). Explicit
// close() reports unload errors; the destructor swallows them (D-010).
// WASM: honest absence.

module;

#include <auxid/macros.hpp>

#include <type_traits>
#include <utility>

export module auxid.dl;

export import auxid.core;
export import auxid.fs;
import auxid.containers;

export namespace au::dl
{
  class AUXID_API Library
  {
public:
    Library() noexcept = default;
    Library(const Library &) = delete;
    auto operator=(const Library &) -> Library & = delete;

    Library(Library &&other) noexcept : m_native(other.m_native)
    {
      other.m_native = nullptr;
    }

    auto operator=(Library &&other) noexcept -> Library &
    {
      if (this != &other)
      {
        (void) close();
        m_native = other.m_native;
        other.m_native = nullptr;
      }
      return *this;
    }

    // Swallows unload errors; use close() to observe them.
    ~Library()
    {
      (void) close();
    }

    [[nodiscard]] static auto open(const filesystem::Path &path) -> Result<Library>;

    // The name must be null-terminated-safe (copied internally).
    [[nodiscard]] auto symbol(StringView name) -> Result<void *>;

    template<typename T> [[nodiscard]] auto symbol_as(StringView name) -> Result<T>
    {
      static_assert(std::is_pointer_v<T>, "symbol_as<T>: T must be a pointer type");
      AU_TRY_VAR(raw, symbol(name));
      return reinterpret_cast<T>(raw);
    }

    auto close() -> Result<void>;

    [[nodiscard]] auto is_open() const noexcept -> bool
    {
      return m_native != nullptr;
    }

private:
    void *m_native = nullptr;
  };
} // namespace au::dl
