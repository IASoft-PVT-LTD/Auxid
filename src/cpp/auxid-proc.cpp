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

namespace au::proc
{
  namespace
  {
#if defined(AU_PLATFORM_WASM)

    [[nodiscard]] auto wasm_absent(const char *op) -> Error
    {
      return Error(String::format("{}: not expressible in a wasm sandbox", op));
    }

#elif AU_PLATFORM_WINDOWS

    [[nodiscard]] auto to_handle(i64 native) noexcept -> void *
    {
      return reinterpret_cast<void *>(static_cast<std::intptr_t>(native));
    }

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

    struct ChildStdio
    {
      void *child_handle = nullptr; // the handle the child receives
      i64 parent_end = -1;          // parent side of a Piped mode, else -1
      bool close_child_after_spawn = false;
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
        void *null_handle =
            ::CreateFileW(L"NUL", win32::GENERIC_READ_ | win32::GENERIC_WRITE_,
                          win32::FILE_SHARE_READ_WRITE_, nullptr, win32::OPEN_EXISTING_, 0, nullptr);
        if (null_handle == win32::invalid_handle())
          return fail(Error::os_last().ctx("proc::spawn: opening NUL"));
        ::SetHandleInformation(null_handle, win32::HANDLE_FLAG_INHERIT_, win32::HANDLE_FLAG_INHERIT_);
        out.child_handle = null_handle;
        out.close_child_after_spawn = true;
        return out;
      }
      case StdioMode::Piped: {
        AU_TRY_VAR(pair, ipc::pipe());
        const i64 child_end = child_reads ? pair.reader.release_native() : pair.writer.release_native();
        out.parent_end = child_reads ? pair.writer.release_native() : pair.reader.release_native();
        ::SetHandleInformation(to_handle(child_end), win32::HANDLE_FLAG_INHERIT_,
                               win32::HANDLE_FLAG_INHERIT_);
        out.child_handle = to_handle(child_end);
        out.close_child_after_spawn = true;
        return out;
      }
      }
      return fail("proc::spawn: unknown StdioMode");
    }

#else // POSIX

    struct ChildStdio
    {
      int child_fd = -1;    // dup2'd onto 0/1/2 in the child
      i64 parent_end = -1;  // parent side of a Piped mode, else -1
      bool close_child_after_spawn = false;
    };

    [[nodiscard]] auto prepare_stdio(StdioMode mode, bool child_reads) -> Result<ChildStdio>
    {
      ChildStdio out;
      switch (mode)
      {
      case StdioMode::Inherit:
        return out; // fork inherits; no dup2 needed
      case StdioMode::Null: {
        const int null_fd = ::open("/dev/null", O_RDWR | O_CLOEXEC);
        if (null_fd < 0)
          return fail(Error::os(errno).ctx("proc::spawn: opening /dev/null"));
        out.child_fd = null_fd;
        out.close_child_after_spawn = true;
        return out;
      }
      case StdioMode::Piped: {
        AU_TRY_VAR(pair, ipc::pipe());
        out.child_fd =
            static_cast<int>(child_reads ? pair.reader.release_native() : pair.writer.release_native());
        out.parent_end = child_reads ? pair.writer.release_native() : pair.reader.release_native();
        out.close_child_after_spawn = true;
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
    void *job = ::CreateJobObjectW(nullptr, nullptr);
    if (job == nullptr)
      return fail(Error::os_last().ctx("proc::LifetimeGroup::create"));

    win32::JobExtendedLimits limits{};
    limits.basic.limit_flags = win32::JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE_;
    if (::SetInformationJobObject(job, win32::JOB_OBJECT_EXTENDED_LIMIT_INFORMATION_, &limits,
                                  sizeof(limits)) == 0)
    {
      Error err = Error::os_last();
      ::CloseHandle(job);
      return fail(std::move(err).ctx("proc::LifetimeGroup::create"));
    }

    LifetimeGroup group;
    group.m_native = static_cast<i64>(reinterpret_cast<std::intptr_t>(job));
    return group;
  }

  AUXID_API auto LifetimeGroup::close() -> void
  {
    if (m_native != -1)
    {
      // KILL_ON_JOB_CLOSE: the kernel terminates every enrolled process.
      ::CloseHandle(to_handle(m_native));
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
      append_quoted_arg(cmdline,
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

    auto cleanup_stdio = [&]() {
      for (ChildStdio *io : {&io_in, &io_out, &io_err})
      {
        if (io->close_child_after_spawn && io->child_handle != nullptr)
          ::CloseHandle(io->child_handle);
        if (io->parent_end != -1)
          ::CloseHandle(to_handle(io->parent_end));
      }
    };

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

    const int created = ::CreateProcessW(
        native_exe.c_str(), wcmdline.data(), nullptr, nullptr, 1, creation_flags,
        opts.env.has_value() ? env_block.data() : nullptr,
        opts.working_dir.has_value() ? opts.working_dir->c_str() : nullptr, &si, &pi);
    if (created == 0)
    {
      Error err = Error::os_last();
      cleanup_stdio();
      return fail(std::move(err).ctx("proc::spawn"));
    }

    if (opts.lifetime != nullptr)
    {
      if (::AssignProcessToJobObject(to_handle(opts.lifetime->m_native), pi.process) == 0)
      {
        Error err = Error::os_last();
        ::TerminateProcess(pi.process, 1);
        ::CloseHandle(pi.thread);
        ::CloseHandle(pi.process);
        cleanup_stdio();
        return fail(std::move(err).ctx("proc::spawn: LifetimeGroup enrollment"));
      }
      ::ResumeThread(pi.thread);
    }
    ::CloseHandle(pi.thread);

    // Child-side handles are the child's now; drop our copies.
    for (ChildStdio *io : {&io_in, &io_out, &io_err})
    {
      if (io->close_child_after_spawn && io->child_handle != nullptr)
        ::CloseHandle(io->child_handle);
    }

    Process process;
    process.m_process = static_cast<i64>(reinterpret_cast<std::intptr_t>(pi.process));
    process.m_id = static_cast<u64>(pi.process_id);
    if (io_in.parent_end != -1)
      process.m_stdin = ipc::PipeWriter::adopt_native(io_in.parent_end);
    if (io_out.parent_end != -1)
      process.m_stdout = ipc::PipeReader::adopt_native(io_out.parent_end);
    if (io_err.parent_end != -1)
      process.m_stderr = ipc::PipeReader::adopt_native(io_err.parent_end);
    return process;
  }

  AUXID_API auto Process::detach() -> void
  {
    if (m_process != -1)
    {
      ::CloseHandle(to_handle(m_process));
      m_process = -1;
    }
  }

  AUXID_API auto Process::wait() -> Result<ExitStatus>
  {
    if (m_reaped)
      return m_last_status;
    if (m_process == -1)
      return fail("proc::Process::wait: no process");

    ::WaitForSingleObject(to_handle(m_process), 0xFFFFFFFFul);
    unsigned long code = 0;
    if (::GetExitCodeProcess(to_handle(m_process), &code) == 0)
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

    const unsigned long waited = ::WaitForSingleObject(to_handle(m_process), 0);
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
    if (::TerminateProcess(to_handle(m_process), 1) == 0)
      return fail(Error::os_last().ctx("proc::Process::terminate"));
    return {};
  }

#else // POSIX

  AUXID_API auto LifetimeGroup::create() -> Result<LifetimeGroup>
  {
    return LifetimeGroup{}; // pid tracking only; Linux children add PDEATHSIG
  }

  AUXID_API auto LifetimeGroup::close() -> void
  {
    for (usize i = 0; i < m_pids.size(); i++)
      ::kill(static_cast<pid_t>(m_pids[i]), SIGKILL); // best effort
    m_pids.clear();
    m_native = -1;
  }

  AUXID_API auto spawn(const filesystem::Path &exe, SpawnOptions opts) -> Result<Process>
  {
    // Everything the child touches is prepared BEFORE fork: between fork and
    // exec only async-signal-safe calls are allowed (no allocation).
    const String exe_path(exe.c_str());

    Vec<char *> argv;
    argv.push_back(const_cast<char *>(exe_path.c_str()));
    for (usize i = 0; i < opts.args.size(); i++)
      argv.push_back(const_cast<char *>(opts.args[i].c_str()));
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
        envp.push_back(const_cast<char *>(env_storage[i].c_str()));
      envp.push_back(nullptr);
    }

    const char *chdir_to = opts.working_dir.has_value() ? opts.working_dir->c_str() : nullptr;

    AU_TRY_VAR(io_in, prepare_stdio(opts.stdin_mode, true));
    AU_TRY_VAR(io_out, prepare_stdio(opts.stdout_mode, false));
    AU_TRY_VAR(io_err, prepare_stdio(opts.stderr_mode, false));

    const bool enroll = opts.lifetime != nullptr;

    const pid_t pid = ::fork();
    if (pid < 0)
    {
      const int saved = errno;
      for (ChildStdio *io : {&io_in, &io_out, &io_err})
      {
        if (io->child_fd != -1)
          ::close(io->child_fd);
        if (io->parent_end != -1)
          ::close(static_cast<int>(io->parent_end));
      }
      return fail(Error::os(saved).ctx("proc::spawn: fork"));
    }

    if (pid == 0)
    {
      // Child: async-signal-safe territory.
#  if defined(AU_PLATFORM_LINUX)
      if (enroll)
        ::prctl(PR_SET_PDEATHSIG, SIGKILL);
#  else
      AU_UNUSED(enroll);
#  endif
      if (io_in.child_fd != -1 && ::dup2(io_in.child_fd, 0) < 0)
        ::_exit(126);
      if (io_out.child_fd != -1 && ::dup2(io_out.child_fd, 1) < 0)
        ::_exit(126);
      if (io_err.child_fd != -1 && ::dup2(io_err.child_fd, 2) < 0)
        ::_exit(126);
      for (ChildStdio *io : {&io_in, &io_out, &io_err})
      {
        if (io->child_fd > 2)
          ::close(io->child_fd);
        if (io->parent_end != -1)
          ::close(static_cast<int>(io->parent_end));
      }
      if (chdir_to != nullptr && ::chdir(chdir_to) != 0)
        ::_exit(126);
      if (opts.env.has_value())
        ::execve(exe_path.c_str(), argv.data(), envp.data());
      else
        ::execv(exe_path.c_str(), argv.data());
      ::_exit(127);
    }

    // Parent: the child ends belong to the child now.
    for (ChildStdio *io : {&io_in, &io_out, &io_err})
    {
      if (io->child_fd != -1)
        ::close(io->child_fd);
    }

    if (opts.lifetime != nullptr)
      opts.lifetime->m_pids.push_back(static_cast<u64>(pid));

    Process process;
    process.m_process = static_cast<i64>(pid);
    process.m_id = static_cast<u64>(pid);
    if (io_in.parent_end != -1)
      process.m_stdin = ipc::PipeWriter::adopt_native(io_in.parent_end);
    if (io_out.parent_end != -1)
      process.m_stdout = ipc::PipeReader::adopt_native(io_out.parent_end);
    if (io_err.parent_end != -1)
      process.m_stderr = ipc::PipeReader::adopt_native(io_err.parent_end);
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
    for (;;)
    {
      const pid_t reaped = ::waitpid(static_cast<pid_t>(m_process), &status, 0);
      if (reaped >= 0)
        break;
      if (errno == EINTR)
        continue;
      return fail(Error::os(errno).ctx("proc::Process::wait"));
    }

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
    const pid_t reaped = ::waitpid(static_cast<pid_t>(m_process), &status, WNOHANG);
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
    if (::kill(static_cast<pid_t>(m_process), SIGKILL) != 0)
      return fail(Error::os(errno).ctx("proc::Process::terminate"));
    return {};
  }

#endif
} // namespace au::proc
