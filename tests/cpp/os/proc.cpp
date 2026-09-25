// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

// OS-CONTRACT conformance: auxid.proc. Children are the platform shell
// (COMSPEC / /bin/sh) — present on every CI runner.

#include <auxid/macros.hpp>

#include <csignal>

import auxid;
import auxid.test;

using namespace au;

namespace
{
  struct ProcBlock final : test::Block
  {
    [[nodiscard]] auto get_name() const -> const char * override
    {
      return "os::proc";
    }

    auto declare_tests() -> void override
    {
#if defined(AU_PLATFORM_WASM)
      add_test("honest_absence", [this] { return honest_absence(); });
#else
      add_test("exit_code", [this] { return exit_code(); });
      add_test("stdout_capture", [this] { return stdout_capture(); });
      add_test("terminate_sleeper", [this] { return terminate_sleeper(); });
      add_test("lifetime_group_kills", [this] { return lifetime_group_kills(); });
#endif
    }

#if defined(AU_PLATFORM_WASM)

    auto honest_absence() -> bool
    {
      proc::SpawnOptions opts;
      return check(proc::spawn(filesystem::Path("/bin/true"), static_cast<proc::SpawnOptions &&>(opts))
                       .is_err(),
                   "spawn is an honest Err on wasm") &&
             check(proc::LifetimeGroup::create().is_err(), "LifetimeGroup is an honest Err on wasm");
    }

#else

    [[nodiscard]] static auto shell_exe() -> filesystem::Path
    {
#  if AU_PLATFORM_WINDOWS
      const String comspec = env::get_or("COMSPEC", "C:\\Windows\\System32\\cmd.exe");
      return filesystem::Path(comspec.c_str());
#  else
      return filesystem::Path("/bin/sh");
#  endif
    }

    [[nodiscard]] static auto shell_opts(const char *script) -> proc::SpawnOptions
    {
      proc::SpawnOptions opts;
#  if AU_PLATFORM_WINDOWS
      opts.args.push_back(String("/c"));
#  else
      opts.args.push_back(String("-c"));
#  endif
      opts.args.push_back(String(script));
      return opts;
    }

    auto exit_code() -> bool
    {
      auto opts = shell_opts("exit 42");
      opts.stdin_mode = proc::StdioMode::Null;
      opts.stdout_mode = proc::StdioMode::Null;
      opts.stderr_mode = proc::StdioMode::Null;

      auto spawned = proc::spawn(shell_exe(), static_cast<proc::SpawnOptions &&>(opts));
      if (!check(spawned.is_ok(), "spawn ok"))
        return false;
      auto process = static_cast<proc::Process &&>(spawned.unwrap());

      auto status = process.wait();
      if (!check(status.is_ok(), "wait ok"))
        return false;
      return check_eq(status.unwrap().code, 42, "exit code observed") &&
             check_not(status.unwrap().abnormal, "normal exit");
    }

    auto stdout_capture() -> bool
    {
      auto opts = shell_opts("echo hello-proc");
      opts.stdin_mode = proc::StdioMode::Null;
      opts.stdout_mode = proc::StdioMode::Piped;
      opts.stderr_mode = proc::StdioMode::Null;

      auto spawned = proc::spawn(shell_exe(), static_cast<proc::SpawnOptions &&>(opts));
      if (!check(spawned.is_ok(), "spawn ok"))
        return false;
      auto process = static_cast<proc::Process &&>(spawned.unwrap());

      String captured;
      u8 buffer[256];
      for (;;)
      {
        auto got = process.stdout_pipe().read(Span<u8>(buffer, sizeof(buffer)));
        if (!check(got.is_ok(), "stdout read ok"))
          return false;
        if (got.unwrap() == 0)
          break; // EOF: child exited and the pipe drained
        captured.append(StringView(reinterpret_cast<const char *>(buffer), got.unwrap()));
      }
      (void) process.wait();

      return check(captured.find("hello-proc") != String::npos, "captured child stdout");
    }

    auto terminate_sleeper() -> bool
    {
#  if AU_PLATFORM_WINDOWS
      auto opts = shell_opts("ping -n 30 127.0.0.1 >NUL");
#  else
      auto opts = shell_opts("sleep 30");
#  endif
      opts.stdin_mode = proc::StdioMode::Null;
      opts.stdout_mode = proc::StdioMode::Null;
      opts.stderr_mode = proc::StdioMode::Null;

      auto spawned = proc::spawn(shell_exe(), static_cast<proc::SpawnOptions &&>(opts));
      if (!check(spawned.is_ok(), "spawn ok"))
        return false;
      auto process = static_cast<proc::Process &&>(spawned.unwrap());

      auto running = process.try_status();
      if (!check(running.is_ok() && !running.unwrap().has_value(), "sleeper is running"))
      {
        (void) process.terminate();
        (void) process.wait();
        return false;
      }

      if (!check(process.terminate().is_ok(), "terminate ok"))
        return false;
      auto status = process.wait();
      if (!check(status.is_ok(), "wait after terminate ok"))
        return false;
#  if AU_PLATFORM_WINDOWS
      return check_eq(status.unwrap().code, 1, "TerminateProcess exit code observed");
#  else
      return check(status.unwrap().abnormal, "signal death is abnormal") &&
             check_eq(status.unwrap().code, static_cast<i32>(SIGKILL), "died by SIGKILL");
#  endif
    }

    auto lifetime_group_kills() -> bool
    {
      auto group_res = proc::LifetimeGroup::create();
      if (!check(group_res.is_ok(), "group created"))
        return false;
      auto group = static_cast<proc::LifetimeGroup &&>(group_res.unwrap());

#  if AU_PLATFORM_WINDOWS
      auto opts = shell_opts("ping -n 30 127.0.0.1 >NUL");
#  else
      auto opts = shell_opts("sleep 30");
#  endif
      opts.stdin_mode = proc::StdioMode::Null;
      opts.stdout_mode = proc::StdioMode::Null;
      opts.stderr_mode = proc::StdioMode::Null;
      opts.lifetime = &group;

      auto spawned = proc::spawn(shell_exe(), static_cast<proc::SpawnOptions &&>(opts));
      if (!check(spawned.is_ok(), "spawn enrolled ok"))
        return false;
      auto process = static_cast<proc::Process &&>(spawned.unwrap());

      group.close(); // must kill the sleeper — otherwise wait() blocks 30s

      auto status = process.wait();
      return check(status.is_ok(), "enrolled child died with the group");
    }

#endif
  };

  const test::AutoRegister<ProcBlock> _registered;
} // namespace
