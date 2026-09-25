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

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <expected>
#include <format>
#include <memory>
#include <mutex>
#include <new>
#include <source_location>
#include <string_view>
#include <type_traits>
#include <utility>

#if defined(_MSC_VER) && !defined(__clang__)
#  include <intrin.h>
#endif

export module auxid.core;

export namespace au
{
  using u8 = std::uint8_t;
  using u16 = std::uint16_t;
  using u32 = std::uint32_t;
  using u64 = std::uint64_t;

  using i8 = std::int8_t;
  using i16 = std::int16_t;
  using i32 = std::int32_t;
  using i64 = std::int64_t;

  using f32 = float;
  using f64 = double;

  using usize = std::size_t;
  using isize = std::ptrdiff_t;
  using alignval = std::align_val_t;

  template<typename T> [[nodiscard]] constexpr decltype(auto) mut(T &&arg) noexcept
  {
    return std::forward<T>(arg);
  }
} // namespace au

export namespace au::compiler
{
  template<class T> constexpr T *addressof(T &arg) noexcept
  {
    return std::addressof(arg);
  }

  template<class T> const T *addressof(const T &&) = delete;

  [[noreturn]] inline void trap() noexcept
  {
#if defined(_MSC_VER) && !defined(__clang__)
    __debugbreak();
    std::abort();
#elif defined(__has_builtin)
#  if __has_builtin(__builtin_trap)
    __builtin_trap();
#  else
    std::abort();
#  endif
#else
    std::abort();
#endif
  }

  inline int memcmp(const void *lhs, const void *rhs, std::size_t n) noexcept
  {
    return std::memcmp(lhs, rhs, n);
  }

  inline std::size_t strlen(const char *s) noexcept
  {
    return std::strlen(s);
  }

  inline const void *memchr(const void *p, int c, std::size_t n) noexcept
  {
    return std::memchr(p, c, n);
  }
} // namespace au::compiler

export namespace au
{
  template<class T> constexpr T *addressof(T &arg) noexcept
  {
    return compiler::addressof(arg);
  }

  template<class T, class... Args> constexpr T *construct_at(T *p, Args &&...args)
  {
    return ::new (static_cast<void *>(p)) T(static_cast<Args &&>(args)...);
  }

  template<class T> constexpr void destroy_at(T *p)
  {
    if constexpr (std::is_array_v<T>)
    {
      for (auto &elem : *p)
        au::destroy_at(compiler::addressof(elem));
    }
    else
    {
      if constexpr (!std::is_trivially_destructible_v<T>)
      {
        p->~T();
      }
    }
  }
} // namespace au

export namespace au
{
  template<typename E> struct Unexpected
  {
    E val;

    constexpr explicit Unexpected(E &&v) : val(std::move(v))
    {
    }

    constexpr explicit Unexpected(const E &v) : val(v)
    {
    }
  };

  template<typename E> [[nodiscard]] constexpr auto fail(E &&error)
  {
    return Unexpected<std::decay_t<E>>(std::forward<E>(error));
  }

  [[noreturn]] extern AUXID_API auto panic_handler(const char *msg, const char *file, u32 line) -> void;

  [[noreturn]] AUXID_API auto panic_at(const char *msg, const char *file, u32 line) -> void;

  [[noreturn]] AUXID_API auto panic(const char *msg, std::source_location loc = std::source_location::current()) -> void;
} // namespace au

export namespace au
{
  template<typename T, typename E> class [[nodiscard]] ResultT
  {
    union {
      T m_val;
      E m_err;
    };

    bool m_is_ok;

public:
    constexpr ResultT(const T &val)
      requires std::is_copy_constructible_v<T>
        : m_val(val), m_is_ok(true)
    {
    }

    constexpr ResultT(T &&val)
      requires std::is_move_constructible_v<T>
        : m_val(std::move(val)), m_is_ok(true)
    {
    }

    template<typename ErrT>
    constexpr ResultT(Unexpected<ErrT> &&failure) : m_err(std::move(failure.val)), m_is_ok(false)
    {
    }

    constexpr ResultT(const std::expected<T, E> &exp) : m_is_ok(exp.has_value())
    {
      if (m_is_ok)
        au::construct_at(&m_val, *exp);
      else
        au::construct_at(&m_err, exp.error());
    }

    constexpr ResultT(std::expected<T, E> &&exp) : m_is_ok(exp.has_value())
    {
      if (m_is_ok)
        au::construct_at(&m_val, std::move(*exp));
      else
        au::construct_at(&m_err, std::move(exp.error()));
    }

    [[nodiscard]] constexpr operator std::expected<T, E>() const &
    {
      if (m_is_ok)
        return std::expected<T, E>{m_val};
      return std::expected<T, E>{std::unexpect, m_err};
    }

    [[nodiscard]] constexpr operator std::expected<T, E>() &&
    {
      if (m_is_ok)
        return std::expected<T, E>{std::move(m_val)};
      return std::expected<T, E>{std::unexpect, std::move(m_err)};
    }

    constexpr ~ResultT()
    {
      if (m_is_ok)
      {
        if constexpr (!std::is_trivially_destructible_v<T>)
          au::destroy_at(&m_val);
      }
      else
      {
        if constexpr (!std::is_trivially_destructible_v<E>)
          au::destroy_at(&m_err);
      }
    }

    constexpr ResultT(const ResultT &other)
      requires(std::is_copy_constructible_v<T> && std::is_copy_constructible_v<E>)
        : m_is_ok(other.m_is_ok)
    {
      if (m_is_ok)
        au::construct_at(&m_val, other.m_val);
      else
        au::construct_at(&m_err, other.m_err);
    }

    constexpr ResultT(ResultT &&other) noexcept
      requires(std::is_move_constructible_v<T> && std::is_move_constructible_v<E>)
        : m_is_ok(other.m_is_ok)
    {
      if (m_is_ok)
        au::construct_at(&m_val, std::move(other.m_val));
      else
        au::construct_at(&m_err, std::move(other.m_err));
    }

    constexpr ResultT &operator=(const ResultT &other)
      requires(std::is_copy_constructible_v<T> && std::is_copy_constructible_v<E>)
    {
      if (this == &other)
        return *this;

      if (m_is_ok && other.m_is_ok)
      {
        m_val = other.m_val;
      }
      else if (!m_is_ok && !other.m_is_ok)
      {
        m_err = other.m_err;
      }
      else
      {
        if (m_is_ok)
        {
          if constexpr (!std::is_trivially_destructible_v<T>)
            au::destroy_at(&m_val);
          au::construct_at(&m_err, other.m_err);
        }
        else
        {
          if constexpr (!std::is_trivially_destructible_v<E>)
            au::destroy_at(&m_err);
          au::construct_at(&m_val, other.m_val);
        }
        m_is_ok = other.m_is_ok;
      }
      return *this;
    }

    constexpr ResultT &operator=(ResultT &&other) noexcept
      requires(std::is_move_constructible_v<T> && std::is_move_constructible_v<E>)
    {
      if (this == &other)
        return *this;

      if (m_is_ok && other.m_is_ok)
      {
        m_val = std::move(other.m_val);
      }
      else if (!m_is_ok && !other.m_is_ok)
      {
        m_err = std::move(other.m_err);
      }
      else
      {
        if (m_is_ok)
        {
          if constexpr (!std::is_trivially_destructible_v<T>)
            au::destroy_at(&m_val);
          au::construct_at(&m_err, std::move(other.m_err));
        }
        else
        {
          if constexpr (!std::is_trivially_destructible_v<E>)
            au::destroy_at(&m_err);
          au::construct_at(&m_val, std::move(other.m_val));
        }
        m_is_ok = other.m_is_ok;
      }
      return *this;
    }

    constexpr T &unwrap(std::source_location loc = std::source_location::current()) &
    {
      if (!m_is_ok)
        au::panic("Called unwrap() on an Error Result", loc);
      return m_val;
    }

    constexpr const T &unwrap(std::source_location loc = std::source_location::current()) const &
    {
      if (!m_is_ok)
        au::panic("Called unwrap() on an Error Result", loc);
      return m_val;
    }

    constexpr T unwrap(std::source_location loc = std::source_location::current()) &&
    {
      if (!m_is_ok)
        au::panic("Called unwrap() on an Error Result", loc);
      return std::move(m_val);
    }

    constexpr const E &unwrap_err(std::source_location loc = std::source_location::current()) const &
    {
      if (m_is_ok)
        au::panic("Called unwrap_err() on an Ok Result", loc);
      return m_err;
    }

    // Non-const overload so std::move(res.unwrap_err()) is a real move: the
    // const& overload alone yields const E&& — which binds to E's COPY
    // constructor. AU_TRY error propagation relies on this overload.
    constexpr E &unwrap_err(std::source_location loc = std::source_location::current()) &
    {
      if (m_is_ok)
        au::panic("Called unwrap_err() on an Ok Result", loc);
      return m_err;
    }

    constexpr const E &err(std::source_location loc = std::source_location::current()) const &
    {
      return unwrap_err(loc);
    }

    constexpr const E &error(std::source_location loc = std::source_location::current()) const &
    {
      return unwrap_err(loc);
    }

    [[nodiscard]] constexpr bool is_ok() const
    {
      return m_is_ok;
    }

    [[nodiscard]] constexpr bool is_err() const
    {
      return !m_is_ok;
    }

    [[nodiscard]] constexpr bool has_value() const
    {
      return m_is_ok;
    }

    constexpr T &operator*() &
    {
      return unwrap();
    }

    constexpr const T &operator*() const &
    {
      return unwrap();
    }

    constexpr T *operator->()
    {
      return &unwrap();
    }

    constexpr const T *operator->() const
    {
      return &unwrap();
    }

    constexpr explicit operator bool() const
    {
      return is_ok();
    }
  };

  template<typename E> class [[nodiscard]] ResultT<void, E>
  {
    union {
      E m_err;
    };

    bool m_is_ok;

public:
    constexpr ResultT() : m_is_ok(true)
    {
    }

    template<typename ErrT>
    constexpr ResultT(Unexpected<ErrT> &&failure) : m_err(std::move(failure.val)), m_is_ok(false)
    {
    }

    constexpr ResultT(ResultT &&other) noexcept : m_is_ok(other.m_is_ok)
    {
      if (!m_is_ok)
        au::construct_at(&m_err, std::move(other.m_err));
    }

    constexpr ResultT(const ResultT &other)
      requires std::is_copy_constructible_v<E>
        : m_is_ok(other.m_is_ok)
    {
      if (!m_is_ok)
        au::construct_at(&m_err, other.m_err);
    }

    constexpr ResultT &operator=(const ResultT &other)
      requires std::is_copy_constructible_v<E>
    {
      if (this == &other)
        return *this;

      if (!m_is_ok && !other.m_is_ok)
      {
        m_err = other.m_err;
      }
      else
      {
        if (!m_is_ok)
        {
          if constexpr (!std::is_trivially_destructible_v<E>)
            au::destroy_at(&m_err);
        }
        if (!other.m_is_ok)
          au::construct_at(&m_err, other.m_err);
        m_is_ok = other.m_is_ok;
      }
      return *this;
    }

    constexpr ResultT &operator=(ResultT &&other) noexcept
      requires std::is_move_constructible_v<E>
    {
      if (this == &other)
        return *this;

      if (!m_is_ok && !other.m_is_ok)
      {
        m_err = std::move(other.m_err);
      }
      else
      {
        if (!m_is_ok)
        {
          if constexpr (!std::is_trivially_destructible_v<E>)
            au::destroy_at(&m_err);
        }
        if (!other.m_is_ok)
          au::construct_at(&m_err, std::move(other.m_err));
        m_is_ok = other.m_is_ok;
      }
      return *this;
    }

    constexpr ResultT(const std::expected<void, E> &exp) : m_is_ok(exp.has_value())
    {
      if (!m_is_ok)
        au::construct_at(&m_err, exp.error());
    }

    constexpr ResultT(std::expected<void, E> &&exp) : m_is_ok(exp.has_value())
    {
      if (!m_is_ok)
        au::construct_at(&m_err, std::move(exp.error()));
    }

    [[nodiscard]] constexpr operator std::expected<void, E>() const &
    {
      if (m_is_ok)
        return std::expected<void, E>{};
      return std::expected<void, E>{std::unexpect, m_err};
    }

    [[nodiscard]] constexpr operator std::expected<void, E>() &&
    {
      if (m_is_ok)
        return std::expected<void, E>{};
      return std::expected<void, E>{std::unexpect, std::move(m_err)};
    }

    constexpr ~ResultT()
    {
      if (!m_is_ok)
      {
        if constexpr (!std::is_trivially_destructible_v<E>)
          au::destroy_at(&m_err);
      }
    }

    constexpr void unwrap(std::source_location loc = std::source_location::current()) const
    {
      if (!m_is_ok)
        au::panic("Called unwrap() on an Error Result", loc);
    }

    [[nodiscard]] constexpr bool is_ok() const
    {
      return m_is_ok;
    }

    [[nodiscard]] constexpr bool is_err() const
    {
      return !m_is_ok;
    }

    [[nodiscard]] constexpr bool has_value() const
    {
      return m_is_ok;
    }

    constexpr const E &unwrap_err(std::source_location loc = std::source_location::current()) const &
    {
      if (m_is_ok)
        au::panic("Called unwrap_err() on an Ok Result", loc);
      return m_err;
    }

    // See the primary template: required for AU_TRY to move rather than copy.
    constexpr E &unwrap_err(std::source_location loc = std::source_location::current()) &
    {
      if (m_is_ok)
        au::panic("Called unwrap_err() on an Ok Result", loc);
      return m_err;
    }

    constexpr const E &err(std::source_location loc = std::source_location::current()) const &
    {
      return unwrap_err(loc);
    }

    constexpr const E &error(std::source_location loc = std::source_location::current()) const &
    {
      return unwrap_err(loc);
    }

    constexpr explicit operator bool() const
    {
      return is_ok();
    }
  };
} // namespace au

// Compile-time facts about the BUILD TARGET. Runtime environment (variables,
// paths) lives in auxid.env / au::env (D-010 ASK-6 ruling).
export namespace au::build
{
#if defined(NDEBUG)
  inline constexpr bool IS_DEBUG = false;
  inline constexpr bool IS_RELEASE = true;
#else
  inline constexpr bool IS_DEBUG = true;
  inline constexpr bool IS_RELEASE = false;
#endif

#if AU_PLATFORM_WINDOWS
  inline constexpr bool IS_WINDOWS = true;
  inline constexpr bool IS_UNIX = false;
#else
  inline constexpr bool IS_WINDOWS = false;
  inline constexpr bool IS_UNIX = true;
#endif

  inline constexpr usize MAX_PATH_LEN = 4096;
} // namespace au::build

export namespace au
{
  struct Version
  {
    u32 major = 0;
    u32 minor = 0;
    u32 patch = 0;

    [[nodiscard]] constexpr auto to_u64() const -> u64
    {
      return (static_cast<u64>(major) << 40) | (static_cast<u64>(minor) << 16) | (static_cast<u64>(patch));
    }
  };
} // namespace au

export namespace au
{
  class Mutex
  {
public:
    Mutex() noexcept = default;
    ~Mutex() = default;
    Mutex(const Mutex &) = delete;
    auto operator=(const Mutex &) -> Mutex & = delete;

    auto lock() -> void
    {
      m_handle.lock();
    }

    auto unlock() -> void
    {
      m_handle.unlock();
    }

    [[nodiscard]] auto try_lock() -> bool
    {
      return m_handle.try_lock();
    }

    auto native_handle() noexcept -> std::mutex &
    {
      return m_handle;
    }

private:
    std::mutex m_handle{};
  };
} // namespace au
