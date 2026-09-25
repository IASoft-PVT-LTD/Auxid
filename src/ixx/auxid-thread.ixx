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

#include <condition_variable>
#include <format>
#include <mutex>
#include <tuple>
#include <type_traits>
#include <utility>

export module auxid.thread;

export import auxid.core;
import auxid.containers;

export namespace au
{
  template<typename MutexType> class LockGuard
  {
public:
    explicit LockGuard(MutexType &m) : m_mutex(m)
    {
      m_mutex.lock();
    }

    ~LockGuard()
    {
      m_mutex.unlock();
    }

    LockGuard(const LockGuard &) = delete;
    auto operator=(const LockGuard &) -> LockGuard & = delete;

private:
    MutexType &m_mutex;
  };

  class ConditionVariable
  {
public:
    ConditionVariable() noexcept = default;
    ~ConditionVariable() = default;
    ConditionVariable(const ConditionVariable &) = delete;
    auto operator=(const ConditionVariable &) -> ConditionVariable & = delete;

    auto notify_one() noexcept -> void
    {
      m_cv.notify_one();
    }

    auto notify_all() noexcept -> void
    {
      m_cv.notify_all();
    }

    auto wait(Mutex &mutex) -> void
    {
      m_cv.wait(mutex);
    }

    template<typename Predicate> auto wait(Mutex &mutex, Predicate stop_waiting) -> void
    {
      m_cv.wait(mutex, std::move(stop_waiting));
    }

private:
    std::condition_variable_any m_cv{};
  };

  class AUXID_API Logger
  {
public:
    enum ELevel
    {
      LEVEL_TRACE,
      LEVEL_DEBUG,
      LEVEL_INFO,
      LEVEL_WARN,
      LEVEL_ERROR
    };

    typedef void (*LogHandler_FuncT)(const char *msg, ELevel level);

public:
    template<typename... Args> auto trace(std::format_string<Args...> fmt, Args &&...args) -> void
    {
      log_impl(LEVEL_TRACE, fmt.get(), std::make_format_args(args...));
    }

    template<typename... Args> auto debug(std::format_string<Args...> fmt, Args &&...args) -> void
    {
      log_impl(LEVEL_DEBUG, fmt.get(), std::make_format_args(args...));
    }

    template<typename... Args> auto info(std::format_string<Args...> fmt, Args &&...args) -> void
    {
      log_impl(LEVEL_INFO, fmt.get(), std::make_format_args(args...));
    }

    template<typename... Args> auto warn(std::format_string<Args...> fmt, Args &&...args) -> void
    {
      log_impl(LEVEL_WARN, fmt.get(), std::make_format_args(args...));
    }

    template<typename... Args> auto error(std::format_string<Args...> fmt, Args &&...args) -> void
    {
      log_impl(LEVEL_ERROR, fmt.get(), std::make_format_args(args...));
    }

public:
    Logger(Mutex &logger_mutex);

    auto set_log_handler(LogHandler_FuncT handler) -> void
    {
      m_handler = handler;
    }

private:
    auto log_impl(ELevel level, StringView fmt, std::format_args args) -> void;
    static auto default_handler(const char *msg, ELevel level) -> void;

    Mutex &m_logger_mutex_ref;
    LogHandler_FuncT m_handler{default_handler};
  };
} // namespace au

export namespace au::auxid
{
  AUXID_API auto initialize_main_thread() -> void;
  AUXID_API auto terminate_main_thread() -> void;
  AUXID_API auto initialize_worker_thread() -> void;
  AUXID_API auto terminate_worker_thread() -> void;

  struct MainThreadGuard
  {
    MainThreadGuard()
    {
      initialize_main_thread();
    }

    ~MainThreadGuard()
    {
      terminate_main_thread();
    }
  };

  struct WorkerThreadGuard
  {
    WorkerThreadGuard()
    {
      initialize_worker_thread();
    }

    ~WorkerThreadGuard()
    {
      terminate_worker_thread();
    }
  };

  AUXID_API auto is_main_thread() -> bool;
  AUXID_API auto is_thread_initialized() -> bool;
  AUXID_API auto get_thread_logger() -> Logger &;
} // namespace au::auxid

export namespace au::detail
{
  using ThreadEntry_FuncT = void (*)(void *);

  struct NativeThread
  {
    void *handle = nullptr; // Win32 HANDLE / pthread_t, opaque to callers
    u64 id = 0;
  };

  // Spawn failure (resource exhaustion, thread limits) is a genuinely
  // recoverable condition, so it surfaces as a real Err — std::thread cannot
  // deliver that under -fno-exceptions (its constructor reports failure by
  // exception only), which is why the backend is native.
  AUXID_API auto spawn_native_thread(ThreadEntry_FuncT entry, void *arg) -> Result<NativeThread>;
  AUXID_API auto join_native_thread(NativeThread &thread) -> void;
  AUXID_API auto current_native_thread_id() noexcept -> u64;
} // namespace au::detail

export namespace au
{
  template<bool JoinOnDestroy> class ThreadT
  {
public:
    using ThreadID = u64;

    template<typename F, typename... Args> static auto create(F &&f, Args &&...args) -> Result<ThreadT>
    {
      struct Invoker
      {
        std::decay_t<F> func;
        std::tuple<std::decay_t<Args>...> tup;
      };

      memory::HeapAllocator heap{};
      void *mem = heap.alloc(sizeof(Invoker), alignof(Invoker));
      Invoker *invoker =
          au::construct_at(static_cast<Invoker *>(mem),
                           Invoker{std::forward<F>(f), std::make_tuple(std::forward<Args>(args)...)});

      const auto entry = +[](void *raw) {
        auxid::WorkerThreadGuard _guard;
        auto *inv = static_cast<Invoker *>(raw);
        std::apply(std::move(inv->func), std::move(inv->tup));
        au::destroy_at(inv);
        memory::HeapAllocator{}.free(inv, sizeof(Invoker), alignof(Invoker));
      };

      auto native = detail::spawn_native_thread(entry, invoker);
      if (native.is_err())
      {
        au::destroy_at(invoker);
        heap.free(invoker, sizeof(Invoker), alignof(Invoker));
        return fail(std::move(native.unwrap_err()));
      }
      return ThreadT(native.unwrap());
    }

    static auto get_calling_thread_id() noexcept -> ThreadID
    {
      return detail::current_native_thread_id();
    }

    ThreadT() noexcept = default;

    ThreadT(const ThreadT &) = delete;
    auto operator=(const ThreadT &) -> ThreadT & = delete;

    ThreadT(ThreadT &&other) noexcept : m_native(other.m_native)
    {
      other.m_native = detail::NativeThread{};
    }

    auto operator=(ThreadT &&other) noexcept -> ThreadT &
    {
      if (this != &other)
      {
        if (joinable())
        {
          if constexpr (JoinOnDestroy)
            join();
          else
            panic("ThreadT move-assigned over a still-joinable thread");
        }
        m_native = other.m_native;
        other.m_native = detail::NativeThread{};
      }
      return *this;
    }

    ~ThreadT()
    {
      if (joinable())
      {
        if constexpr (JoinOnDestroy)
          join();
        else
          panic("ThreadT destroyed while still joinable (use JThread or call join())");
      }
    }

    [[nodiscard]] auto joinable() const noexcept -> bool
    {
      return m_native.handle != nullptr;
    }

    auto join() -> void
    {
      if (joinable())
        detail::join_native_thread(m_native);
    }

    [[nodiscard]] auto get_id() const noexcept -> ThreadID
    {
      return m_native.id;
    }

private:
    explicit ThreadT(detail::NativeThread native) noexcept : m_native(native)
    {
    }

    detail::NativeThread m_native{};
  };

  using Thread = ThreadT<false>;
  using JThread = ThreadT<true>;
} // namespace au
