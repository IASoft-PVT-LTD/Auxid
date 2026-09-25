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

#include <cmath>
#include <concepts>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>

export module auxid.test;

import auxid.core;
import auxid.containers;

export namespace au::console
{
  inline constexpr std::string_view RESET = "\033[0m";
  inline constexpr std::string_view RED = "\033[31m";
  inline constexpr std::string_view GREEN = "\033[32m";
  inline constexpr std::string_view YELLOW = "\033[33m";
  inline constexpr std::string_view BLUE = "\033[34m";
  inline constexpr std::string_view MAGENTA = "\033[35m";
  inline constexpr std::string_view CYAN = "\033[36m";
} // namespace au::console

export namespace au::test
{
  template<typename T>
  concept HasToStringMethod = requires(const T &t) {
    { t.to_string() } -> std::convertible_to<String>;
  };

  template<typename T> auto to_string(const T &value) -> String
  {
    using Decayed = std::decay_t<T>;

    if constexpr (std::is_pointer_v<Decayed>)
    {
      if constexpr (std::is_same_v<Decayed, const char *> || std::is_same_v<Decayed, char *>)
      {
        if (value == nullptr)
          return String("\"nullptr\"");
        return String("\"") + String(value) + "\"";
      }
      else
      {
        if (value == nullptr)
          return String("nullptr");
        char buf[32];
        std::snprintf(buf, sizeof(buf), "ptr(%p)", static_cast<const void *>(value));
        return String(buf);
      }
    }
    else if constexpr (std::is_arithmetic_v<T>)
    {
      if constexpr (std::is_same_v<Decayed, bool>)
        return Decayed{value} ? String("true") : String("false");
      else if constexpr (std::is_integral_v<Decayed>)
      {
        using Wide = std::conditional_t<std::is_signed_v<Decayed>, long long, unsigned long long>;
        return String(std::to_string(static_cast<Wide>(value)).c_str());
      }
      else
        return String(std::to_string(static_cast<long double>(value)).c_str());
    }
    else if constexpr (std::is_convertible_v<T, String>)
    {
      return String("\"") + String(value) + "\"";
    }
    else if constexpr (HasToStringMethod<T>)
    {
      return value.to_string();
    }
    else
    {
      return String("{Object}");
    }
  }

  class TestFunctor
  {
    static constexpr usize kBufSize = sizeof(void *) * 2;
    using InvokeFn = bool (*)(const void *);

    InvokeFn m_invoke = nullptr;
    alignas(void *) std::byte m_buf[kBufSize]{};

public:
    TestFunctor() = default;

    template<class F>
      requires std::invocable<F> && std::same_as<std::invoke_result_t<F>, bool> && std::is_trivially_copyable_v<F> &&
               (sizeof(F) <= kBufSize) && (alignof(F) <= alignof(void *))
    TestFunctor(F f) noexcept : m_invoke(&invoke_impl<F>)
    {
      au::construct_at(reinterpret_cast<F *>(&m_buf), std::move(f));
    }

    TestFunctor(const TestFunctor &other) noexcept : m_invoke(other.m_invoke)
    {
      if (m_invoke != nullptr)
        std::memcpy(m_buf, other.m_buf, kBufSize);
    }

    TestFunctor(TestFunctor &&other) noexcept : m_invoke(other.m_invoke)
    {
      if (m_invoke != nullptr)
        std::memcpy(m_buf, other.m_buf, kBufSize);
      other.m_invoke = nullptr;
    }

    auto operator=(const TestFunctor &other) noexcept -> TestFunctor &
    {
      if (this != &other)
      {
        m_invoke = other.m_invoke;
        if (m_invoke != nullptr)
          std::memcpy(m_buf, other.m_buf, kBufSize);
      }
      return *this;
    }

    auto operator=(TestFunctor &&other) noexcept -> TestFunctor &
    {
      if (this != &other)
      {
        m_invoke = other.m_invoke;
        if (m_invoke != nullptr)
          std::memcpy(m_buf, other.m_buf, kBufSize);
        other.m_invoke = nullptr;
      }
      return *this;
    }

    [[nodiscard]] bool operator()() const
    {
      return m_invoke(&m_buf);
    }

private:
    template<class F>
    static bool invoke_impl(const void *p) noexcept(noexcept((*static_cast<const F *>(p))()))
    {
      return (*static_cast<const F *>(p))();
    }
  };

  struct TestUnit
  {
    String name;
    TestFunctor functor;
  };

  class Block
  {
public:
    virtual ~Block() = default;
    [[nodiscard]] virtual auto get_name() const -> const char * = 0;
    virtual auto declare_tests() -> void = 0;

    auto units() -> Vec<TestUnit> &
    {
      return m_units;
    }

    template<typename T1, typename T2> auto check_eq(const T1 &lhs, const T2 &rhs, const char *description) -> bool
    {
      if (lhs != rhs)
      {
        print_fail(description, to_string(lhs), to_string(rhs));
        return false;
      }
      return true;
    }

    template<typename T1, typename T2> auto check_neq(const T1 &lhs, const T2 &rhs, const char *description) -> bool
    {
      if (lhs == rhs)
      {
        print_fail(description, to_string(lhs), "NOT " + to_string(rhs));
        return false;
      }
      return true;
    }

    template<typename T>
    auto check_approx(const T lhs, const T rhs, const char *description, const T epsilon = static_cast<T>(0.001))
        -> bool
    {
      static_assert(std::is_floating_point_v<T>, "check_approx only works for floats/doubles");

      if (lhs == static_cast<T>(0.0) || rhs == static_cast<T>(0.0))
      {
        if (std::abs(lhs - rhs) > epsilon)
        {
          print_fail(description, to_string(lhs), to_string(rhs));
          return false;
        }
        return true;
      }

      const T diff = std::abs(lhs - rhs);
      const T larger = std::max(std::abs(lhs), std::abs(rhs));

      if (diff > (larger * epsilon))
      {
        print_fail(description, to_string(lhs), to_string(rhs));
        return false;
      }
      return true;
    }

    auto check(const bool value, const char *description) -> bool
    {
      if (!value)
      {
        std::println("{}    {}... {}FAILED{}", console::BLUE, std::string_view(description), console::RED,
                     console::RESET);
        return false;
      }
      return true;
    }

    auto check_not(const bool value, const char *description) -> bool
    {
      if (value)
      {
        std::println("{}    {}... {}FAILED{}", console::BLUE, std::string_view(description), console::RED,
                     console::RESET);
        return false;
      }
      return true;
    }

    auto add_test(const char *name, TestFunctor functor) -> void
    {
      m_units.push_back({String(name), std::move(functor)});
    }

private:
    auto print_fail(const char *desc, const String &v1, const String &v2) -> void
    {
      std::println("{}    {}... {}FAILED\n      Expected: {}\n      Actual:   {}{}", console::BLUE,
                   std::string_view(desc), console::RED, v2, v1, console::RESET);
    }

    Vec<TestUnit> m_units;
  };

  template<typename T>
  concept ValidBlockClass = std::derived_from<T, Block>;

  namespace impl
  {
    AUXID_API auto print_block_header(std::string_view name) -> void;
    AUXID_API auto print_test_progress(std::string_view name) -> void;
    AUXID_API auto print_blank_line() -> void;
    AUXID_API auto print_discovered(usize count) -> void;
    AUXID_API auto print_summary(usize fail_count, usize test_count, usize block_count) -> void;
  } // namespace impl

  template<bool StopOnFail = false, bool IsVerbose = false> class Runner
  {
public:
    Runner() = default;

    ~Runner()
    {
      summarize();
    }

    template<typename BlockClass>
      requires ValidBlockClass<BlockClass>
    auto test_block() -> void;

    [[nodiscard]] auto fail_count() const noexcept -> usize
    {
      return m_fail_count;
    }

    [[nodiscard]] auto test_count() const noexcept -> usize
    {
      return m_test_count;
    }

private:
    auto summarize() -> void;

    usize m_test_count{0};
    usize m_fail_count{0};
    usize m_block_count{0};
  };

  template<bool StopOnFail, bool IsVerbose>
  template<typename BlockClass>
    requires ValidBlockClass<BlockClass>
  auto Runner<StopOnFail, IsVerbose>::test_block() -> void
  {
    m_block_count++;
    BlockClass b;
    b.declare_tests();

    impl::print_block_header(std::string_view(b.get_name()));

    Vec<TestUnit> &units = b.units();
    for (usize i = 0; i < units.size(); ++i)
    {
      TestUnit &v = units[i];
      m_test_count++;
      if constexpr (IsVerbose)
      {
        impl::print_test_progress(std::string_view(v.name.data(), v.name.size()));
      }

      const bool result = v.functor();

      if (!result)
      {
        m_fail_count++;
        if constexpr (StopOnFail)
        {
          summarize();
          std::exit(-1);
        }
      }
    }
    impl::print_blank_line();
  }

  template<bool StopOnFail, bool IsVerbose> auto Runner<StopOnFail, IsVerbose>::summarize() -> void
  {
    impl::print_summary(m_fail_count, m_test_count, m_block_count);
  }

  using DefaultRunner = Runner<false, true>;

  class AUXID_API TestRegistry
  {
public:
    using TestEntry = void (*)(DefaultRunner &);

    static auto get_entries() -> Vec<TestEntry> &;
    static auto run_all() -> i32;
  };

  AUXID_API auto register_test_block(TestRegistry::TestEntry entry) -> void;

  template<typename BlockType> void invoke_test_block(DefaultRunner &r)
  {
    r.test_block<BlockType>();
  }

  template<typename BlockType> struct AutoRegister
  {
    AutoRegister()
    {
      register_test_block(&invoke_test_block<BlockType>);
    }
  };
} // namespace au::test
