// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

// OS Foundation Layer: processes (docs/OS-CONTRACT.md). Spawn supervised
// children with piped/inherited/null stdio; LifetimeGroup is the portable
// kill-on-close supervision primitive (Windows job object; PDEATHSIG plus
// group-kill on Linux; group-kill only on macOS — the parent-crash
// guarantee is honestly weaker there, see the contract). Enrollment happens
// at spawn time only, so it is atomic with process creation.
// WASM: honest absence — every operation returns an Err.

module;

#include <auxid/macros.hpp>

export module auxid.proc;

export import auxid.core;
export import auxid.ipc;
import auxid.containers;
import auxid.fs;

export namespace au::proc
{
  enum class StdioMode : u8
  {
    Inherit, // child shares this process's stream
    Piped,   // a pipe; the parent end is exposed on Process
    Null,    // NUL / /dev/null
  };

  struct ExitStatus
  {
    i32 code = 0;
    // Best-effort: signal death on POSIX (code = signal number);
    // NTSTATUS-range exit codes on Windows.
    bool abnormal = false;
  };

  class Process;
  struct SpawnOptions;
  [[nodiscard]] AUXID_API auto spawn(const filesystem::Path &exe, SpawnOptions opts)
      -> Result<Process>;

  class AUXID_API LifetimeGroup
  {
public:
    LifetimeGroup() noexcept = default;
    LifetimeGroup(const LifetimeGroup &) = delete;
    auto operator=(const LifetimeGroup &) -> LifetimeGroup & = delete;

    LifetimeGroup(LifetimeGroup &&other) noexcept
        : m_native(other.m_native), m_pids(static_cast<Vec<u64> &&>(other.m_pids))
    {
      other.m_native = -1;
    }

    auto operator=(LifetimeGroup &&other) noexcept -> LifetimeGroup &
    {
      if (this != &other)
      {
        close();
        m_native = other.m_native;
        m_pids = static_cast<Vec<u64> &&>(other.m_pids);
        other.m_native = -1;
      }
      return *this;
    }

    // Destroying the group kills every enrolled child that is still alive.
    ~LifetimeGroup()
    {
      close();
    }

    [[nodiscard]] static auto create() -> Result<LifetimeGroup>;

    auto close() -> void;

private:
    friend class Process;
    friend auto spawn(const filesystem::Path &exe, SpawnOptions opts) -> Result<Process>;

    i64 m_native = -1;   // Windows: job object handle; POSIX: unused
    Vec<u64> m_pids{};   // POSIX: enrolled pids, killed on close
  };

  struct SpawnOptions
  {
    Vec<String> args{};                            // argv[1..]
    Option<Vec<Pair<String, String>>> env{};       // none = inherit parent env
    Option<filesystem::Path> working_dir{};        // none = inherit
    StdioMode stdin_mode = StdioMode::Inherit;
    StdioMode stdout_mode = StdioMode::Inherit;
    StdioMode stderr_mode = StdioMode::Inherit;
    LifetimeGroup *lifetime = nullptr;             // enroll atomically at spawn
  };

  class AUXID_API Process
  {
public:
    Process() noexcept = default;
    Process(const Process &) = delete;
    auto operator=(const Process &) -> Process & = delete;

    Process(Process &&other) noexcept
        : m_process(other.m_process), m_id(other.m_id), m_reaped(other.m_reaped),
          m_last_status(other.m_last_status), m_stdin(static_cast<ipc::PipeWriter &&>(other.m_stdin)),
          m_stdout(static_cast<ipc::PipeReader &&>(other.m_stdout)),
          m_stderr(static_cast<ipc::PipeReader &&>(other.m_stderr))
    {
      other.m_process = -1;
      other.m_id = 0;
    }

    auto operator=(Process &&other) noexcept -> Process &
    {
      if (this != &other)
      {
        detach();
        m_process = other.m_process;
        m_id = other.m_id;
        m_reaped = other.m_reaped;
        m_last_status = other.m_last_status;
        m_stdin = static_cast<ipc::PipeWriter &&>(other.m_stdin);
        m_stdout = static_cast<ipc::PipeReader &&>(other.m_stdout);
        m_stderr = static_cast<ipc::PipeReader &&>(other.m_stderr);
        other.m_process = -1;
        other.m_id = 0;
      }
      return *this;
    }

    // Destruction DETACHES (closes handles; the child keeps running).
    // Kill-on-drop is LifetimeGroup's job, not the destructor's.
    ~Process()
    {
      detach();
    }

    // Close our handle to the child without affecting the child.
    auto detach() -> void;

    [[nodiscard]] auto id() const noexcept -> u64
    {
      return m_id;
    }

    // Parent ends of Piped stdio; is_open() is false for non-Piped modes.
    [[nodiscard]] auto stdin_pipe() noexcept -> ipc::PipeWriter &
    {
      return m_stdin;
    }

    [[nodiscard]] auto stdout_pipe() noexcept -> ipc::PipeReader &
    {
      return m_stdout;
    }

    [[nodiscard]] auto stderr_pipe() noexcept -> ipc::PipeReader &
    {
      return m_stderr;
    }

    // Blocking; idempotent (the status is cached after the first wait).
    [[nodiscard]] auto wait() -> Result<ExitStatus>;

    // Non-blocking poll: none = still running.
    [[nodiscard]] auto try_status() -> Result<Option<ExitStatus>>;

    // Hard kill (TerminateProcess / SIGKILL). Graceful shutdown is an IPC
    // message + this as the fallback (D-010: no request_exit).
    auto terminate() -> Result<void>;

private:
    friend auto spawn(const filesystem::Path &exe, SpawnOptions opts) -> Result<Process>;

    i64 m_process = -1; // Windows: process handle; POSIX: pid
    u64 m_id = 0;
    bool m_reaped = false;
    ExitStatus m_last_status{};
    ipc::PipeWriter m_stdin{};
    ipc::PipeReader m_stdout{};
    ipc::PipeReader m_stderr{};
  };

} // namespace au::proc
