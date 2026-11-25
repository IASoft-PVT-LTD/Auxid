// auxid: trust-level 1
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
#include <cstdlib>
#include <cstring>
// Textual <filesystem>: Path codegen in a module impl unit (same Homebrew
// LLVM 22.1.8 hazard as auxid-env.cpp).
#include <filesystem>
#include <string_view>
#include <utility>

#if !AU_PLATFORM_WINDOWS
#  include <fcntl.h>
#  include <signal.h>
#  include <sys/wait.h>
#  include <unistd.h>
#  if defined(AU_PLATFORM_LINUX)
#    include <sys/prctl.h>
#  endif
#endif

module auxid.proc;

import auxid.memory;
import auxid.containers;
import auxid.thread;

namespace au::proc
{
  namespace
  {
#if defined(AU_PLATFORM_WASM)

    [[nodiscard]] auto wasm_absent(const char *op) -> Error
    {
      return Error(String::format("{}: not expressible in a wasm sandbox", op));
    }

#else

#  if AU_PLATFORM_WINDOWS
    [[nodiscard]] auto to_handle(i64 native) noexcept -> void *
    {
      // SAFETY: a HANDLE stored as an integer; it is only passed back to the OS.
      AU_UNSAFE return reinterpret_cast<void *>(static_cast<std::intptr_t>(native));
    }

    auto close_native(i64 native) -> void
    {
      // SAFETY: the one caller owns native, an open handle, and forgets it.
      AU_UNSAFE ::CloseHandle(to_handle(native));
    }

#  else
    auto close_native(i64 native) -> void
    {
      ::close(static_cast<int>(native));
    }

#  endif

    // An open handle or descriptor, closed on destruction: spawn gives up its copies on every path.
    class OwnedNative
    {
public:
      OwnedNative() noexcept = default;

      explicit OwnedNative(i64 native) noexcept : m_native(native)
      {
      }

      OwnedNative(const OwnedNative &) = delete;
      auto operator=(const OwnedNative &) -> OwnedNative & = delete;

      OwnedNative(OwnedNative &&other) noexcept : m_native(other.release())
      {
      }

      auto operator=(OwnedNative &&other) noexcept -> OwnedNative &
      {
        if (this != &other)
        {
          close();
          m_native = other.release();
        }
        return *this;
      }

      ~OwnedNative()
      {
        close();
      }

      [[nodiscard]] auto is_open() const noexcept -> bool
      {
        return m_native != -1;
      }

      [[nodiscard]] auto get() const noexcept -> i64
      {
        return m_native;
      }

      [[nodiscard]] auto release() noexcept -> i64
      {
        const i64 native = m_native;
        m_native = -1;
        return native;
      }

      auto close() -> void
      {
        if (is_open())
          close_native(release());
      }

private:
      i64 m_native = -1;
    };

#endif

#if AU_PLATFORM_WINDOWS

    // MSVC command-line quoting rules: backslashes double only before a
    // quote; the argument is wrapped in quotes when it contains separators.
    auto append_quoted_arg(String &cmdline, StringView arg) -> void
    {
      const bool needs_quotes =
          arg.empty() || arg.find(' ') != StringView::npos || arg.find('\t') != StringView::npos ||
          arg.find('"') != StringView::npos;
      if (!needs_quotes)
      {
        cmdline.append(arg);
        return;
      }

      cmdline.append("\"");
      usize backslashes = 0;
      for (usize i = 0; i < arg.size(); i++)
      {
        const char c = arg[i];
        if (c == '\\')
        {
          backslashes++;
          continue;
        }
        if (c == '"')
        {
          for (usize b = 0; b < backslashes * 2 + 1; b++)
            cmdline.append("\\");
          backslashes = 0;
          cmdline.append("\"");
          continue;
        }
        for (usize b = 0; b < backslashes; b++)
          cmdline.append("\\");
        backslashes = 0;
        cmdline.append(StringView(&c, 1));
      }
      for (usize b = 0; b < backslashes * 2; b++)
        cmdline.append("\\");
      cmdline.append("\"");
    }

    [[nodiscard]] auto widen_z(StringView utf8) -> Vec<wchar_t>
    {
      Vec<wchar_t> wide;
      if (!utf8.empty())
      {
        // SAFETY: reads utf8.size() bytes (int limits that to 2 GB) and only counts.
        AU_UNSAFE const int needed = ::MultiByteToWideChar(win32::CP_UTF8_, 0, utf8.data(),
                                                 static_cast<int>(utf8.size()), nullptr, 0);
        if (needed > 0)
        {
          wide.resize(static_cast<usize>(needed));
          // SAFETY: the same input, written to the `needed` units just allocated.
          AU_UNSAFE ::MultiByteToWideChar(win32::CP_UTF8_, 0, utf8.data(), static_cast<int>(utf8.size()),
                                wide.data(), needed);
        }
      }
      wide.push_back(L'\0');
      return wide;
    }

    struct ChildStdio
    {
      void *child_handle = nullptr; // the handle the child receives
      OwnedNative child_end{};      // that handle, unless it is inherited from this process
      OwnedNative parent_end{};     // parent side of a Piped mode
    };

    [[nodiscard]] auto prepare_stdio(StdioMode mode, unsigned long std_handle_id, bool child_reads)
        -> Result<ChildStdio>
    {
      ChildStdio out;
      switch (mode)
      {
      case StdioMode::Inherit:
        out.child_handle = ::GetStdHandle(std_handle_id);
        return out;
      case StdioMode::Null: {
        // SAFETY: the name is a literal; no other pointer is passed.
        AU_UNSAFE void *null_handle =
            ::CreateFileW(L"NUL", win32::GENERIC_READ_ | win32::GENERIC_WRITE_,
                          win32::FILE_SHARE_READ_WRITE_, nullptr, win32::OPEN_EXISTING_, 0, nullptr);
        if (null_handle == win32::invalid_handle())
          return fail(Error::os_last().ctx("proc::spawn: opening NUL"));
        // SAFETY: a HANDLE fits in 64 bits; to_handle reverses this.
        AU_UNSAFE out.child_end = OwnedNative(static_cast<i64>(reinterpret_cast<std::intptr_t>(null_handle)));
        // SAFETY: null_handle was just opened.
        AU_UNSAFE ::SetHandleInformation(null_handle, win32::HANDLE_FLAG_INHERIT_, win32::HANDLE_FLAG_INHERIT_);
        out.child_handle = null_handle;
        return out;
      }
      case StdioMode::Piped: {
        AU_TRY_VAR(pair, ipc::pipe());
        out.child_end = OwnedNative(child_reads ? pair.reader.release_native() : pair.writer.release_native());
        out.parent_end = OwnedNative(child_reads ? pair.writer.release_native() : pair.reader.release_native());
        out.child_handle = to_handle(out.child_end.get());
        // SAFETY: child_handle is an open pipe handle.
        AU_UNSAFE ::SetHandleInformation(out.child_handle, win32::HANDLE_FLAG_INHERIT_, win32::HANDLE_FLAG_INHERIT_);
        return out;
      }
      }
      return fail("proc::spawn: unknown StdioMode");
    }

#elif !defined(AU_PLATFORM_WASM)

    struct ChildStdio
    {
      OwnedNative child_end{};  // dup2'd onto 0/1/2 in the child
      OwnedNative parent_end{}; // parent side of a Piped mode
    };

    [[nodiscard]] auto fd_of(const OwnedNative &end) noexcept -> int
    {
      return static_cast<int>(end.get());
    }

    [[nodiscard]] auto prepare_stdio(StdioMode mode, bool child_reads) -> Result<ChildStdio>
    {
      ChildStdio out;
      switch (mode)
      {
      case StdioMode::Inherit:
        return out; // fork inherits; no dup2 needed
      case StdioMode::Null: {
        // SAFETY: the path is a literal, and without O_CREAT no mode is read.
        AU_UNSAFE const int null_fd = ::open("/dev/null", O_RDWR | O_CLOEXEC);
        if (null_fd < 0)
          return fail(Error::os(errno).ctx("proc::spawn: opening /dev/null"));
        out.child_end = OwnedNative(null_fd);
        return out;
      }
      case StdioMode::Piped: {
        AU_TRY_VAR(pair, ipc::pipe());
        out.child_end = OwnedNative(child_reads ? pair.reader.release_native() : pair.writer.release_native());
        out.parent_end = OwnedNative(child_reads ? pair.writer.release_native() : pair.reader.release_native());
        return out;
      }
      }
      return fail("proc::spawn: unknown StdioMode");
    }

#endif
  } // namespace

#if defined(AU_PLATFORM_WASM)

  AUXID_API auto LifetimeGroup::create() -> Result<LifetimeGroup>
  {
    return fail(wasm_absent("proc::LifetimeGroup::create"));
  }

  AUXID_API auto LifetimeGroup::close() -> void
  {
    m_native = -1;
  }

  AUXID_API auto spawn(const filesystem::Path &, SpawnOptions) -> Result<Process>
  {
    return fail(wasm_absent("proc::spawn"));
  }

  AUXID_API auto Process::detach() -> void
  {
    m_process = -1;
  }

  AUXID_API auto Process::wait() -> Result<ExitStatus>
  {
    return fail(wasm_absent("proc::Process::wait"));
  }

  AUXID_API auto Process::try_status() -> Result<Option<ExitStatus>>
  {
    return fail(wasm_absent("proc::Process::try_status"));
  }

  AUXID_API auto Process::terminate() -> Result<void>
  {
    return fail(wasm_absent("proc::Process::terminate"));
  }

#elif AU_PLATFORM_WINDOWS

  AUXID_API auto LifetimeGroup::create() -> Result<LifetimeGroup>
  {
    // SAFETY: null means default security and no name.
    AU_UNSAFE void *job = ::CreateJobObjectW(nullptr, nullptr);
    if (job == nullptr)
      return fail(Error::os_last().ctx("proc::LifetimeGroup::create"));

    win32::JobExtendedLimits limits{};
    limits.basic.limit_flags = win32::JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE_;
    // SAFETY: limits has the layout this information class requires (checked in auxid_win32.hpp).
    AU_UNSAFE const int limited = ::SetInformationJobObject(job, win32::JOB_OBJECT_EXTENDED_LIMIT_INFORMATION_, &limits,
                                                            sizeof(limits));
    if (limited == 0)
    {
      Error err = Error::os_last();
      // SAFETY: the job was just created and has no other owner.
      AU_UNSAFE ::CloseHandle(job);
      return fail(std::move(err).ctx("proc::LifetimeGroup::create"));
    }

    LifetimeGroup group;
    // SAFETY: a HANDLE fits in 64 bits; to_handle reverses this.
    AU_UNSAFE group.m_native = static_cast<i64>(reinterpret_cast<std::intptr_t>(job));
    return group;
  }

  AUXID_API auto LifetimeGroup::close() -> void
  {
    if (m_native != -1)
    {
      // KILL_ON_JOB_CLOSE: the kernel terminates every enrolled process.
      // SAFETY: m_native is an open job this object owns; it is cleared below, so it is closed once.
      AU_UNSAFE ::CloseHandle(to_handle(m_native));
      m_native = -1;
    }
  }

  AUXID_API auto spawn(const filesystem::Path &exe, SpawnOptions opts) -> Result<Process>
  {
    // Command line: quoted argv[0] (the exe path) then quoted args.
    const std::filesystem::path::string_type &native_exe = exe.native();
    String cmdline;
    {
      // Path -> utf8 for quoting (exe paths are expected to be encodable).
      const auto exe_utf8 = exe.u8string();
      // SAFETY: char may read any object's bytes, and the view covers exactly exe_utf8.
      AU_UNSAFE append_quoted_arg(cmdline,
                        StringView(reinterpret_cast<const char *>(exe_utf8.data()), exe_utf8.size()));
      for (usize i = 0; i < opts.args.size(); i++)
      {
        cmdline.append(" ");
        append_quoted_arg(cmdline, StringView(opts.args[i].data(), opts.args[i].size()));
      }
    }
    Vec<wchar_t> wcmdline = widen_z(StringView(cmdline.data(), cmdline.size()));

    AU_TRY_VAR(io_in, prepare_stdio(opts.stdin_mode, win32::STD_INPUT_HANDLE_, true));
    AU_TRY_VAR(io_out, prepare_stdio(opts.stdout_mode, win32::STD_OUTPUT_HANDLE_, false));
    AU_TRY_VAR(io_err, prepare_stdio(opts.stderr_mode, win32::STD_ERROR_HANDLE_, false));

    // Optional replacement environment: UTF-16 double-null-terminated block.
    Vec<wchar_t> env_block;
    unsigned long creation_flags = 0;
    if (opts.env.has_value())
    {
      String joined;
      for (usize i = 0; i < opts.env->size(); i++)
      {
        joined.append(StringView((*opts.env)[i].first.data(), (*opts.env)[i].first.size()));
        joined.append("=");
        joined.append(StringView((*opts.env)[i].second.data(), (*opts.env)[i].second.size()));
        joined.append(StringView("\0", 1));
      }
      env_block = widen_z(StringView(joined.data(), joined.size()));
      env_block.push_back(L'\0'); // block must end double-null even when empty
      creation_flags |= win32::CREATE_UNICODE_ENVIRONMENT_;
    }
    if (opts.lifetime != nullptr)
      creation_flags |= win32::CREATE_SUSPENDED_;

    win32::StartupInfoW si{};
    si.cb = sizeof(si);
    si.flags = win32::STARTF_USESTDHANDLES_;
    si.std_input = io_in.child_handle;
    si.std_output = io_out.child_handle;
    si.std_error = io_err.child_handle;

    win32::ProcessInformation pi{};

    // SAFETY: the strings and the environment block are null-terminated and outlive the call, wcmdline is writable as
    // required, and si and pi have the layout of the SDK structs (checked in auxid_win32.hpp).
    AU_UNSAFE const int created = ::CreateProcessW(
        native_exe.c_str(), wcmdline.data(), nullptr, nullptr, 1, creation_flags,
        opts.env.has_value() ? env_block.data() : nullptr,
        opts.working_dir.has_value() ? opts.working_dir->c_str() : nullptr, &si, &pi);
    if (created == 0)
    {
      Error err = Error::os_last();
      return fail(std::move(err).ctx("proc::spawn"));
    }

    if (opts.lifetime != nullptr)
    {
      // SAFETY: the caller keeps the group alive for this call; both handles are open.
      AU_UNSAFE const int enrolled = ::AssignProcessToJobObject(to_handle(opts.lifetime->m_native), pi.process);
      if (enrolled == 0)
      {
        Error err = Error::os_last();
        // SAFETY: both handles are open, as CreateProcessW returned them, and not used after this.
        AU_UNSAFE
        {
          ::TerminateProcess(pi.process, 1);
          ::CloseHandle(pi.thread);
          ::CloseHandle(pi.process);
        }
        return fail(std::move(err).ctx("proc::spawn: LifetimeGroup enrollment"));
      }
      // SAFETY: pi.thread is the open handle CreateProcessW returned.
      AU_UNSAFE ::ResumeThread(pi.thread);
    }
    // SAFETY: pi.thread is open, and this is its only close on this path.
    AU_UNSAFE ::CloseHandle(pi.thread);

    Process process;
    // SAFETY: a HANDLE fits in 64 bits; to_handle reverses this.
    AU_UNSAFE process.m_process = static_cast<i64>(reinterpret_cast<std::intptr_t>(pi.process));
    process.m_id = static_cast<u64>(pi.process_id);
    if (io_in.parent_end.is_open())
      process.m_stdin = ipc::PipeWriter::adopt_native(io_in.parent_end.release());
    if (io_out.parent_end.is_open())
      process.m_stdout = ipc::PipeReader::adopt_native(io_out.parent_end.release());
    if (io_err.parent_end.is_open())
      process.m_stderr = ipc::PipeReader::adopt_native(io_err.parent_end.release());
    return process;
  }

  AUXID_API auto Process::detach() -> void
  {
    if (m_process != -1)
    {
      // SAFETY: m_process is an open handle this object owns; it is cleared below, so it is closed once.
      AU_UNSAFE ::CloseHandle(to_handle(m_process));
      m_process = -1;
    }
  }

  AUXID_API auto Process::wait() -> Result<ExitStatus>
  {
    if (m_reaped)
      return m_last_status;
    if (m_process == -1)
      return fail("proc::Process::wait: no process");

    // SAFETY: m_process is an open process handle.
    AU_UNSAFE ::WaitForSingleObject(to_handle(m_process), 0xFFFFFFFFul);
    unsigned long code = 0;
    // SAFETY: m_process is an open process handle; the call writes the code.
    AU_UNSAFE const int known = ::GetExitCodeProcess(to_handle(m_process), &code);
    if (known == 0)
      return fail(Error::os_last().ctx("proc::Process::wait"));

    m_last_status = ExitStatus{static_cast<i32>(code), (code & 0xC0000000ul) == 0xC0000000ul};
    m_reaped = true;
    return m_last_status;
  }

  AUXID_API auto Process::try_status() -> Result<Option<ExitStatus>>
  {
    if (m_reaped)
      return Option<ExitStatus>(m_last_status);
    if (m_process == -1)
      return fail("proc::Process::try_status: no process");

    // SAFETY: m_process is an open process handle.
    AU_UNSAFE const unsigned long waited = ::WaitForSingleObject(to_handle(m_process), 0);
    if (waited == win32::WAIT_TIMEOUT_)
      return Option<ExitStatus>{};
    if (waited != win32::WAIT_OBJECT_0_)
      return fail(Error::os_last().ctx("proc::Process::try_status"));

    AU_TRY_VAR(status, wait());
    return Option<ExitStatus>(status);
  }

  AUXID_API auto Process::terminate() -> Result<void>
  {
    if (m_process == -1)
      return fail("proc::Process::terminate: no process");
    if (m_reaped)
      return fail("proc::Process::terminate: the process has exited");
    // SAFETY: m_process is an open process handle.
    AU_UNSAFE const int killed = ::TerminateProcess(to_handle(m_process), 1);
    if (killed == 0)
      return fail(Error::os_last().ctx("proc::Process::terminate"));
    return {};
  }

#else // POSIX

  namespace
  {
    // waitpid, with errno set on -1 and 0 while the child runs (only if `block` is false). A child of a group is
    // reaped under the group's lock and leaves it, so LifetimeGroup::close() cannot signal a recycled pid.
    [[nodiscard]] auto reap(pid_t pid, const Arc<detail::Children> &group, bool block, int &status) -> pid_t
    {
      if (!group)
      {
        pid_t reaped = -1;
        do
        {
          // SAFETY: writes one int into status.
          AU_UNSAFE reaped = ::waitpid(pid, &status, block ? 0 : WNOHANG);
        } while (reaped < 0 && errno == EINTR);
        return reaped;
      }

      // Blocks without reaping, so the lock is not held meanwhile.
      while (block)
      {
        siginfo_t info{};
        // SAFETY: writes one siginfo_t into info.
        AU_UNSAFE const int waited = ::waitid(P_PID, static_cast<id_t>(pid), &info, WEXITED | WNOWAIT);
        if (waited == 0)
          break;
        if (errno != EINTR)
          return -1;
      }

      LockGuard<Mutex> lock(group->mutex);
      // SAFETY: writes one int into status.
      AU_UNSAFE const pid_t reaped = ::waitpid(pid, &status, WNOHANG);
      if (reaped == pid)
      {
        for (usize i = 0; i < group->pids.size(); i++)
        {
          if (group->pids[i] != static_cast<u64>(pid))
            continue;
          group->pids[i] = group->pids.back();
          group->pids.pop_back();
          break;
        }
      }
      return reaped;
    }
  } // namespace

  AUXID_API auto LifetimeGroup::create() -> Result<LifetimeGroup>
  {
    return LifetimeGroup{}; // pid tracking only; Linux children add PDEATHSIG
  }

  AUXID_API auto LifetimeGroup::close() -> void
  {
    if (m_children)
    {
      LockGuard<Mutex> lock(m_children->mutex);
      for (const u64 pid : m_children->pids)
        ::kill(static_cast<pid_t>(pid), SIGKILL); // best effort
      m_children->pids.clear();
    }
    m_native = -1;
  }

  AUXID_API auto spawn(const filesystem::Path &exe, SpawnOptions opts) -> Result<Process>
  {
    // Everything the child touches is prepared BEFORE fork: between fork and
    // exec only async-signal-safe calls are allowed (no allocation).
    const String exe_path(exe.c_str());

    Vec<char *> argv;
    // SAFETY: exec takes char *const[] for historical reasons and does not write to the strings.
    AU_UNSAFE argv.push_back(const_cast<char *>(exe_path.c_str()));
    for (usize i = 0; i < opts.args.size(); i++)
    {
      // SAFETY: as above.
      AU_UNSAFE argv.push_back(const_cast<char *>(opts.args[i].c_str()));
    }
    argv.push_back(nullptr);

    Vec<String> env_storage;
    Vec<char *> envp;
    if (opts.env.has_value())
    {
      for (usize i = 0; i < opts.env->size(); i++)
      {
        String entry((*opts.env)[i].first.data(), (*opts.env)[i].first.size());
        entry.append("=");
        entry.append(StringView((*opts.env)[i].second.data(), (*opts.env)[i].second.size()));
        env_storage.push_back(static_cast<String &&>(entry));
      }
      for (usize i = 0; i < env_storage.size(); i++)
      {
        // SAFETY: as for argv; env_storage is complete, so the strings no longer move.
        AU_UNSAFE envp.push_back(const_cast<char *>(env_storage[i].c_str()));
      }
      envp.push_back(nullptr);
    }

    const char *chdir_to = opts.working_dir.has_value() ? opts.working_dir->c_str() : nullptr;

    AU_TRY_VAR(io_in, prepare_stdio(opts.stdin_mode, true));
    AU_TRY_VAR(io_out, prepare_stdio(opts.stdout_mode, false));
    AU_TRY_VAR(io_err, prepare_stdio(opts.stderr_mode, false));

    Arc<detail::Children> group;
    if (opts.lifetime != nullptr)
    {
      // SAFETY: not null, and the caller keeps the group alive for this call.
      AU_UNSAFE
      {
        if (!opts.lifetime->m_children)
          opts.lifetime->m_children = make_arc<detail::Children>();
        group = opts.lifetime->m_children;
      }
    }
    const bool enroll = static_cast<bool>(group);

    const pid_t pid = ::fork();
    if (pid < 0)
      return fail(Error::os(errno).ctx("proc::spawn: fork"));

    if (pid == 0)
    {
      // Child: async-signal-safe territory.
#  if defined(AU_PLATFORM_LINUX)
      if (enroll)
        ::prctl(PR_SET_PDEATHSIG, SIGKILL);
#  else
      AU_UNUSED(enroll);
#  endif
      if (io_in.child_end.is_open() && ::dup2(fd_of(io_in.child_end), 0) < 0)
        ::_exit(126);
      if (io_out.child_end.is_open() && ::dup2(fd_of(io_out.child_end), 1) < 0)
        ::_exit(126);
      if (io_err.child_end.is_open() && ::dup2(fd_of(io_err.child_end), 2) < 0)
        ::_exit(126);
      // SAFETY: the three locals above outlive the loop.
      AU_UNSAFE for (const ChildStdio *io : {&io_in, &io_out, &io_err})
      {
        if (fd_of(io->child_end) > 2)
          ::close(fd_of(io->child_end));
        if (io->parent_end.is_open())
          ::close(fd_of(io->parent_end));
      }
      // SAFETY: chdir_to is null-terminated and points into opts.working_dir.
      AU_UNSAFE if (chdir_to != nullptr && ::chdir(chdir_to) != 0)
        ::_exit(126);
      if (opts.env.has_value())
      {
        // SAFETY: argv and envp end in null and point into strings that are alive in the child's copy of memory.
        AU_UNSAFE ::execve(exe_path.c_str(), argv.data(), envp.data());
      }
      else
      {
        // SAFETY: argv ends in null and points into strings that are alive in the child's copy of memory.
        AU_UNSAFE ::execv(exe_path.c_str(), argv.data());
      }
      ::_exit(127);
    }

    if (group)
    {
      LockGuard<Mutex> lock(group->mutex);
      group->pids.push_back(static_cast<u64>(pid));
    }

    Process process;
    process.m_process = static_cast<i64>(pid);
    process.m_id = static_cast<u64>(pid);
    process.m_children = static_cast<Arc<detail::Children> &&>(group);
    if (io_in.parent_end.is_open())
      process.m_stdin = ipc::PipeWriter::adopt_native(io_in.parent_end.release());
    if (io_out.parent_end.is_open())
      process.m_stdout = ipc::PipeReader::adopt_native(io_out.parent_end.release());
    if (io_err.parent_end.is_open())
      process.m_stderr = ipc::PipeReader::adopt_native(io_err.parent_end.release());
    return process;
  }

  AUXID_API auto Process::detach() -> void
  {
    m_process = -1; // a pid is not a resource; the child keeps running
  }

  AUXID_API auto Process::wait() -> Result<ExitStatus>
  {
    if (m_reaped)
      return m_last_status;
    if (m_process == -1)
      return fail("proc::Process::wait: no process");

    int status = 0;
    if (reap(static_cast<pid_t>(m_process), m_children, true, status) < 0)
      return fail(Error::os(errno).ctx("proc::Process::wait"));

    if (WIFEXITED(status))
      m_last_status = ExitStatus{WEXITSTATUS(status), false};
    else if (WIFSIGNALED(status))
      m_last_status = ExitStatus{WTERMSIG(status), true};
    else
      m_last_status = ExitStatus{-1, true};
    m_reaped = true;
    return m_last_status;
  }

  AUXID_API auto Process::try_status() -> Result<Option<ExitStatus>>
  {
    if (m_reaped)
      return Option<ExitStatus>(m_last_status);
    if (m_process == -1)
      return fail("proc::Process::try_status: no process");

    int status = 0;
    const pid_t reaped = reap(static_cast<pid_t>(m_process), m_children, false, status);
    if (reaped == 0)
      return Option<ExitStatus>{};
    if (reaped < 0)
      return fail(Error::os(errno).ctx("proc::Process::try_status"));

    if (WIFEXITED(status))
      m_last_status = ExitStatus{WEXITSTATUS(status), false};
    else if (WIFSIGNALED(status))
      m_last_status = ExitStatus{WTERMSIG(status), true};
    else
      m_last_status = ExitStatus{-1, true};
    m_reaped = true;
    return Option<ExitStatus>(m_last_status);
  }

  AUXID_API auto Process::terminate() -> Result<void>
  {
    if (m_process == -1)
      return fail("proc::Process::terminate: no process");
    if (m_reaped)
      return fail("proc::Process::terminate: the process has exited");
    if (::kill(static_cast<pid_t>(m_process), SIGKILL) != 0)
      return fail(Error::os(errno).ctx("proc::Process::terminate"));
    return {};
  }

#endif
} // namespace au::proc
