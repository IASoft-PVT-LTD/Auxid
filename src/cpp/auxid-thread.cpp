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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <print>

#if !AU_PLATFORM_WINDOWS
#  include <pthread.h>
#endif

#if !defined(AUXID_USE_SYSTEM_MALLOC)
#  include <rpmalloc/rpmalloc.h>
#endif

module auxid.thread;

import auxid.memory;

// Win32/CRT entry points come from <auxid_win32.hpp> — the shared manual
// prototypes; <windows.h> is banned from module-unit global fragments.

namespace au::detail
{
  namespace
  {
    struct EntryBaton
    {
      ThreadEntry_FuncT entry;
      void *arg;
    };

    auto alloc_baton(ThreadEntry_FuncT entry, void *arg) -> EntryBaton *
    {
      auto *baton =
          static_cast<EntryBaton *>(memory::HeapAllocator{}.alloc(sizeof(EntryBaton), alignof(EntryBaton)));
      baton->entry = entry;
      baton->arg = arg;
      return baton;
    }

    // Runs on the NEW thread, before any guard exists — safe because of the
    // HeapAllocator foreign-thread guarantee (D-006).
    auto consume_baton(void *raw) -> EntryBaton
    {
      auto *baton = static_cast<EntryBaton *>(raw);
      const EntryBaton local = *baton;
      memory::HeapAllocator{}.free(baton, sizeof(EntryBaton), alignof(EntryBaton));
      return local;
    }

#if AU_PLATFORM_WINDOWS
    unsigned __stdcall thread_entry_adapter(void *raw)
    {
      const EntryBaton baton = consume_baton(raw);
      baton.entry(baton.arg);
      return 0;
    }
#else
    auto thread_entry_adapter(void *raw) -> void *
    {
      const EntryBaton baton = consume_baton(raw);
      baton.entry(baton.arg);
      return nullptr;
    }
#endif
  } // namespace

#if AU_PLATFORM_WINDOWS

  AUXID_API auto spawn_native_thread(ThreadEntry_FuncT entry, void *arg) -> Result<NativeThread>
  {
    EntryBaton *baton = alloc_baton(entry, arg);

    unsigned int thread_id = 0;
    const unsigned long long handle = ::_beginthreadex(nullptr, 0, &thread_entry_adapter, baton, 0, &thread_id);
    if (handle == 0)
    {
      const int spawn_errno = errno;
      memory::HeapAllocator{}.free(baton, sizeof(EntryBaton), alignof(EntryBaton));
      return fail(Error(ErrorDomain::Os, spawn_errno, String("thread spawn failed (_beginthreadex)")));
    }

    return NativeThread{reinterpret_cast<void *>(handle), static_cast<u64>(thread_id)};
  }

  AUXID_API auto join_native_thread(NativeThread &thread) -> void
  {
    constexpr unsigned long K_WAIT_INFINITE = 0xFFFFFFFFul;
    ::WaitForSingleObject(thread.handle, K_WAIT_INFINITE);
    ::CloseHandle(thread.handle);
    thread = NativeThread{};
  }

  AUXID_API auto current_native_thread_id() noexcept -> u64
  {
    return static_cast<u64>(::GetCurrentThreadId());
  }

#else

  AUXID_API auto spawn_native_thread(ThreadEntry_FuncT entry, void *arg) -> Result<NativeThread>
  {
    EntryBaton *baton = alloc_baton(entry, arg);

    pthread_t handle{};
    const int rc = ::pthread_create(&handle, nullptr, &thread_entry_adapter, baton);
    if (rc != 0)
    {
      memory::HeapAllocator{}.free(baton, sizeof(EntryBaton), alignof(EntryBaton));
      return fail(Error(ErrorDomain::Os, rc, String("thread spawn failed (pthread_create)")));
    }

    return NativeThread{reinterpret_cast<void *>(handle), static_cast<u64>(reinterpret_cast<std::uintptr_t>(
                                                              reinterpret_cast<void *>(handle)))};
  }

  AUXID_API auto join_native_thread(NativeThread &thread) -> void
  {
    ::pthread_join(reinterpret_cast<pthread_t>(thread.handle), nullptr);
    thread = NativeThread{};
  }

  AUXID_API auto current_native_thread_id() noexcept -> u64
  {
    return static_cast<u64>(reinterpret_cast<std::uintptr_t>(reinterpret_cast<void *>(::pthread_self())));
  }

#endif
} // namespace au::detail

namespace au::auxid
{
  struct ThreadData
  {
    i32 init_counter{};
    memory::Box<Logger> logger;
  };

#if !defined(AUXID_USE_SYSTEM_MALLOC)
  struct RpmallocLifetime
  {
    RpmallocLifetime() noexcept
    {
      rpmalloc_initialize(nullptr);
    }

    ~RpmallocLifetime()
    {
      rpmalloc_finalize();
    }

    RpmallocLifetime(const RpmallocLifetime &) = delete;
    RpmallocLifetime &operator=(const RpmallocLifetime &) = delete;
  };
#endif

  struct State
  {
#if !defined(AUXID_USE_SYSTEM_MALLOC)
    RpmallocLifetime rpmalloc_lifetime{};
#endif
    Mutex logger_mutex{};
    // Guards main_thread_id and thread_data: worker guards run on their own
    // threads, so the registry is touched concurrently.
    Mutex registry_mutex{};
    Thread::ThreadID main_thread_id{};
    HashMap<Thread::ThreadID, ThreadData> thread_data{};
  };

  static auto get_state() -> State &
  {
    static State s_state{};
    return s_state;
  }

  AUXID_API auto initialize_main_thread() -> void
  {
    auto &state = get_state();
    const auto thread_id = Thread::get_calling_thread_id();

    LockGuard<Mutex> lock(state.registry_mutex);
    auto &data = state.thread_data[thread_id];
    data.init_counter++;
    if (data.init_counter > 1)
      return;

    state.main_thread_id = thread_id;
    data.logger = memory::make_box<Logger>(state.logger_mutex);
  }

  AUXID_API auto terminate_main_thread() -> void
  {
    auto &state = get_state();
    const auto thread_id = Thread::get_calling_thread_id();

    LockGuard<Mutex> lock(state.registry_mutex);
    auto &data = state.thread_data[thread_id];
    data.init_counter--;
    if (data.init_counter > 0)
      return;

    data.logger.reset();
  }

  AUXID_API auto initialize_worker_thread() -> void
  {
#if !defined(AUXID_USE_SYSTEM_MALLOC)
    // Before any allocation on this thread — the registry work below
    // allocates. (Lazy init would also cover it; explicit is the normal path.)
    rpmalloc_thread_initialize();
#endif

    auto &state = get_state();
    const auto thread_id = Thread::get_calling_thread_id();

    LockGuard<Mutex> lock(state.registry_mutex);
    auto &data = state.thread_data[thread_id];
    data.init_counter++;
    if (data.init_counter > 1)
      return;

    data.logger = memory::make_box<Logger>(state.logger_mutex);
  }

  AUXID_API auto terminate_worker_thread() -> void
  {
    auto &state = get_state();
    const auto thread_id = Thread::get_calling_thread_id();

    {
      LockGuard<Mutex> lock(state.registry_mutex);
      auto &data = state.thread_data[thread_id];
      data.init_counter--;
      if (data.init_counter > 0)
        return;

      data.logger.reset();
    }

#if !defined(AUXID_USE_SYSTEM_MALLOC)
    rpmalloc_thread_finalize();
#endif
  }

  AUXID_API auto is_main_thread() -> bool
  {
    auto &state = get_state();
    LockGuard<Mutex> lock(state.registry_mutex);
    return state.main_thread_id == Thread::get_calling_thread_id();
  }

  AUXID_API auto is_thread_initialized() -> bool
  {
    auto &state = get_state();
    LockGuard<Mutex> lock(state.registry_mutex);
    const auto *data = state.thread_data.find(Thread::get_calling_thread_id());
    return data != nullptr && data->init_counter > 0;
  }

  AUXID_API auto get_thread_logger() -> Logger &
  {
    auto &state = get_state();
    LockGuard<Mutex> lock(state.registry_mutex);
    auto *data = state.thread_data.find(Thread::get_calling_thread_id());
    if (data == nullptr || !data->logger)
      panic("get_thread_logger() on a thread with no guard (foreign threads have no logger)");
    return *data->logger;
  }
} // namespace au::auxid

namespace au
{
#define CC_RESET "\033[0m"
#define CC_RED "\033[31m"
#define CC_GREEN "\033[32m"
#define CC_YELLOW "\033[33m"
#define CC_BLUE "\033[34m"
#define CC_MAGENTA "\033[35m"
#define CC_CYAN "\033[36m"

  AUXID_API Logger::Logger(Mutex &logger_mutex) : m_logger_mutex_ref(logger_mutex)
  {
  }

  AUXID_API auto Logger::log_impl(ELevel level, StringView fmt, std::format_args args) -> void
  {
    const auto msg = String::vformat(fmt, args);
    m_logger_mutex_ref.lock();
    m_handler(msg.c_str(), level);
    m_logger_mutex_ref.unlock();
  }

  AUXID_API auto Logger::default_handler(const char *msg, ELevel level) -> void
  {
    switch (level)
    {
    case LEVEL_TRACE:
      std::println(stdout, CC_RESET "[TRCE]: {}" CC_RESET, msg);
      break;
    case LEVEL_DEBUG:
      std::println(stdout, CC_CYAN "[DBUG]: {}" CC_RESET, msg);
      break;
    case LEVEL_INFO:
      std::println(stdout, CC_GREEN "[INFO]: {}" CC_RESET, msg);
      break;
    case LEVEL_WARN:
      std::println(stdout, CC_YELLOW "[WARN]: {}" CC_RESET, msg);
      break;
    case LEVEL_ERROR:
      std::println(stdout, CC_RED "[EROR]: {}" CC_RESET, msg);
      break;
    }
  }
} // namespace au
