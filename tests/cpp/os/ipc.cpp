// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

// OS-CONTRACT conformance: auxid.ipc.

#include <auxid/macros.hpp>

#include <cstring>

import auxid;
import auxid.test;

using namespace au;

namespace
{
  [[nodiscard]] auto as_bytes(const char *text) -> Span<const u8>
  {
    return Span<const u8>(reinterpret_cast<const u8 *>(text), std::strlen(text));
  }

  struct IpcBlock final : test::Block
  {
    [[nodiscard]] auto get_name() const -> const char * override
    {
      return "os::ipc";
    }

    auto declare_tests() -> void override
    {
#if defined(AU_PLATFORM_WASM)
      add_test("honest_absence", [this] { return honest_absence(); });
#else
      add_test("pipe_roundtrip_and_eof", [this] { return pipe_roundtrip_and_eof(); });
      add_test("channel_roundtrip", [this] { return channel_roundtrip(); });
      add_test("instance_lock", [this] { return instance_lock(); });
#endif
    }

#if defined(AU_PLATFORM_WASM)

    auto honest_absence() -> bool
    {
      // Contract: no processes in a wasm sandbox; every ipc op is an
      // honest Err.
      return check(ipc::pipe().is_err(), "pipe() is an honest Err on wasm") &&
             check(ipc::ChannelListener::bind("x").is_err(), "bind() is an honest Err on wasm") &&
             check(ipc::InstanceLock::acquire("x").is_err(), "acquire() is an honest Err on wasm");
    }

#else

    auto pipe_roundtrip_and_eof() -> bool
    {
      auto pair = ipc::pipe();
      if (!check(pair.is_ok(), "pipe created"))
        return false;
      auto [reader, writer] = std::move(pair.unwrap());

      if (!check(writer.write_all(as_bytes("hello-pipe")).is_ok(), "write_all ok"))
        return false;

      u8 buffer[64] = {};
      auto got = reader.read(Span<u8>(buffer, sizeof(buffer)));
      if (!check(got.is_ok(), "read ok"))
        return false;
      if (!check_eq(String(reinterpret_cast<const char *>(buffer), got.unwrap()), "hello-pipe",
                    "payload round-trips"))
        return false;

      writer.close();
      auto eof = reader.read(Span<u8>(buffer, sizeof(buffer)));
      return check(eof.is_ok(), "read after writer close is Ok") &&
             check_eq(eof.unwrap(), usize{0}, "Ok(0) = end of stream (contract)");
    }

    auto channel_roundtrip() -> bool
    {
      const String name = String::format("test-chan-{}", time::monotonic_ns());

      auto listener_res = ipc::ChannelListener::bind(StringView(name.data(), name.size()));
      if (!check(listener_res.is_ok(), "listener bound"))
        return false;
      auto listener = std::move(listener_res.unwrap());

      bool client_ok = false;
      auto client_res = Thread::create([&name, &client_ok] {
        auto chan_res = ipc::Channel::connect(StringView(name.data(), name.size()));
        if (chan_res.is_err())
          return;
        auto chan = std::move(chan_res.unwrap());
        if (chan.write_all(as_bytes("ping")).is_err())
          return;
        u8 buffer[16] = {};
        auto got = chan.read(Span<u8>(buffer, sizeof(buffer)));
        client_ok = got.is_ok() &&
                    String(reinterpret_cast<const char *>(buffer), got.unwrap()) == "pong";
      });
      if (!check(client_res.is_ok(), "client thread spawned"))
        return false;
      Thread client = std::move(client_res.unwrap());

      auto server_res = listener.accept();
      if (!check(server_res.is_ok(), "accept ok"))
      {
        client.join();
        return false;
      }
      auto server = std::move(server_res.unwrap());

      u8 buffer[16] = {};
      auto got = server.read(Span<u8>(buffer, sizeof(buffer)));
      const bool got_ping =
          got.is_ok() && String(reinterpret_cast<const char *>(buffer), got.unwrap()) == "ping";
      const bool sent_pong = server.write_all(as_bytes("pong")).is_ok();

      client.join();

      return check(got_ping, "server received ping") && check(sent_pong, "server sent pong") &&
             check(client_ok, "client received pong");
    }

    auto instance_lock() -> bool
    {
      const String key = String::format("test-lock-{}", time::monotonic_ns());
      const StringView key_view(key.data(), key.size());

      auto first = ipc::InstanceLock::acquire(key_view);
      if (!check(first.is_ok(), "first acquire ok"))
        return false;

      auto second = ipc::InstanceLock::acquire(key_view);
      if (!check(second.is_err(), "second acquire fails while held"))
        return false;
      if (!check(ipc::is_already_held(second.unwrap_err()), "classifier: is_already_held"))
        return false;

      first.unwrap().release();
      auto third = ipc::InstanceLock::acquire(key_view);
      return check(third.is_ok(), "re-acquire after release ok");
    }

#endif
  };

  const test::AutoRegister<IpcBlock> _registered;
} // namespace
