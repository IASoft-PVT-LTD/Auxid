// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

// OS Foundation Layer: local IPC (docs/OS-CONTRACT.md). Byte streams only —
// framing is the caller's business; product protocols (and the planned
// shared-memory ring fast path) layer on top. Names are app-local: the
// per-OS realization (named pipe vs unix domain socket path) is defined in
// the contract. WASM: honest absence — every operation returns an Err.
//
// Handles are owning, movable, non-copyable. adopt_native/release_native
// are the deliberate low-level seam (auxid.proc wires child stdio through
// them); the native value is a Win32 HANDLE or POSIX fd widened to i64.

module;

#include <auxid/macros.hpp>

export module auxid.ipc;

export import auxid.core;
import auxid.containers;

export namespace au::ipc
{
  inline constexpr i64 K_INVALID_IPC_HANDLE = -1;

  class AUXID_API PipeReader
  {
public:
    PipeReader() noexcept = default;
    PipeReader(const PipeReader &) = delete;
    auto operator=(const PipeReader &) -> PipeReader & = delete;

    PipeReader(PipeReader &&other) noexcept : m_native(other.m_native)
    {
      other.m_native = K_INVALID_IPC_HANDLE;
    }

    auto operator=(PipeReader &&other) noexcept -> PipeReader &
    {
      if (this != &other)
      {
        close();
        m_native = other.m_native;
        other.m_native = K_INVALID_IPC_HANDLE;
      }
      return *this;
    }

    ~PipeReader()
    {
      close();
    }

    // Blocking. Ok(0) = end of stream (all writers closed).
    auto read(Span<u8> buffer) -> Result<usize>;

    auto close() -> void;

    [[nodiscard]] auto is_open() const noexcept -> bool
    {
      return m_native != K_INVALID_IPC_HANDLE;
    }

    [[nodiscard]] static auto adopt_native(i64 native) noexcept -> PipeReader
    {
      PipeReader r;
      r.m_native = native;
      return r;
    }

    [[nodiscard]] auto release_native() noexcept -> i64
    {
      const i64 native = m_native;
      m_native = K_INVALID_IPC_HANDLE;
      return native;
    }

private:
    i64 m_native = K_INVALID_IPC_HANDLE;
  };

  class AUXID_API PipeWriter
  {
public:
    PipeWriter() noexcept = default;
    PipeWriter(const PipeWriter &) = delete;
    auto operator=(const PipeWriter &) -> PipeWriter & = delete;

    PipeWriter(PipeWriter &&other) noexcept : m_native(other.m_native)
    {
      other.m_native = K_INVALID_IPC_HANDLE;
    }

    auto operator=(PipeWriter &&other) noexcept -> PipeWriter &
    {
      if (this != &other)
      {
        close();
        m_native = other.m_native;
        other.m_native = K_INVALID_IPC_HANDLE;
      }
      return *this;
    }

    ~PipeWriter()
    {
      close();
    }

    // Blocking. Returns bytes accepted (may be a short write).
    auto write(Span<const u8> data) -> Result<usize>;

    // Convenience: writes the whole span, looping over short writes.
    auto write_all(Span<const u8> data) -> Result<void>;

    auto close() -> void;

    [[nodiscard]] auto is_open() const noexcept -> bool
    {
      return m_native != K_INVALID_IPC_HANDLE;
    }

    [[nodiscard]] static auto adopt_native(i64 native) noexcept -> PipeWriter
    {
      PipeWriter w;
      w.m_native = native;
      return w;
    }

    [[nodiscard]] auto release_native() noexcept -> i64
    {
      const i64 native = m_native;
      m_native = K_INVALID_IPC_HANDLE;
      return native;
    }

private:
    i64 m_native = K_INVALID_IPC_HANDLE;
  };

  struct PipePair
  {
    PipeReader reader;
    PipeWriter writer;
  };

  // Anonymous unidirectional pipe (child stdio, readiness handshakes).
  [[nodiscard]] AUXID_API auto pipe() -> Result<PipePair>;

  // Bidirectional local byte-stream channel: named pipe (Windows) / unix
  // domain socket (POSIX). Owner-only permissions per the contract.
  class AUXID_API Channel
  {
public:
    Channel() noexcept = default;
    Channel(const Channel &) = delete;
    auto operator=(const Channel &) -> Channel & = delete;

    Channel(Channel &&other) noexcept : m_native(other.m_native)
    {
      other.m_native = K_INVALID_IPC_HANDLE;
    }

    auto operator=(Channel &&other) noexcept -> Channel &
    {
      if (this != &other)
      {
        close();
        m_native = other.m_native;
        other.m_native = K_INVALID_IPC_HANDLE;
      }
      return *this;
    }

    ~Channel()
    {
      close();
    }

    [[nodiscard]] static auto connect(StringView name) -> Result<Channel>;

    // Blocking. Ok(0) = peer closed.
    auto read(Span<u8> buffer) -> Result<usize>;

    // Blocking; SIGPIPE-safe on POSIX. Peer-closed surfaces as an Err for
    // which is_closed() is true.
    auto write(Span<const u8> data) -> Result<usize>;

    auto write_all(Span<const u8> data) -> Result<void>;

    auto close() -> void;

    [[nodiscard]] auto is_open() const noexcept -> bool
    {
      return m_native != K_INVALID_IPC_HANDLE;
    }

    [[nodiscard]] static auto adopt_native(i64 native) noexcept -> Channel
    {
      Channel c;
      c.m_native = native;
      return c;
    }

private:
    i64 m_native = K_INVALID_IPC_HANDLE;
  };

  class AUXID_API ChannelListener
  {
public:
    ChannelListener() noexcept = default;
    ChannelListener(const ChannelListener &) = delete;
    auto operator=(const ChannelListener &) -> ChannelListener & = delete;

    ChannelListener(ChannelListener &&other) noexcept
        : m_native(other.m_native), m_name(static_cast<String &&>(other.m_name))
    {
      other.m_native = K_INVALID_IPC_HANDLE;
    }

    auto operator=(ChannelListener &&other) noexcept -> ChannelListener &
    {
      if (this != &other)
      {
        close();
        m_native = other.m_native;
        m_name = static_cast<String &&>(other.m_name);
        other.m_native = K_INVALID_IPC_HANDLE;
      }
      return *this;
    }

    ~ChannelListener()
    {
      close();
    }

    [[nodiscard]] static auto bind(StringView name) -> Result<ChannelListener>;

    // Blocking; one Channel per client connection.
    [[nodiscard]] auto accept() -> Result<Channel>;

    auto close() -> void;

    [[nodiscard]] auto is_open() const noexcept -> bool
    {
      return m_native != K_INVALID_IPC_HANDLE;
    }

private:
    i64 m_native = K_INVALID_IPC_HANDLE;
    String m_name{};
  };

  // Single-instance arbitration. Ok = this process is the primary and holds
  // the lock until the object dies; an Err for which is_already_held() is
  // true = a primary already exists (connect to its channel instead).
  class AUXID_API InstanceLock
  {
public:
    InstanceLock() noexcept = default;
    InstanceLock(const InstanceLock &) = delete;
    auto operator=(const InstanceLock &) -> InstanceLock & = delete;

    InstanceLock(InstanceLock &&other) noexcept
        : m_native(other.m_native), m_path(static_cast<String &&>(other.m_path))
    {
      other.m_native = K_INVALID_IPC_HANDLE;
    }

    auto operator=(InstanceLock &&other) noexcept -> InstanceLock &
    {
      if (this != &other)
      {
        release();
        m_native = other.m_native;
        m_path = static_cast<String &&>(other.m_path);
        other.m_native = K_INVALID_IPC_HANDLE;
      }
      return *this;
    }

    ~InstanceLock()
    {
      release();
    }

    [[nodiscard]] static auto acquire(StringView app_key) -> Result<InstanceLock>;

    auto release() -> void;

    [[nodiscard]] auto is_held() const noexcept -> bool
    {
      return m_native != K_INVALID_IPC_HANDLE;
    }

private:
    i64 m_native = K_INVALID_IPC_HANDLE;
    String m_path{};
  };

  // Portable classifiers over raw OS error codes (the codes stay primary —
  // contract). is_closed: the peer is gone (broken pipe / connection reset).
  // is_already_held: InstanceLock::acquire lost to an existing primary.
  [[nodiscard]] AUXID_API auto is_closed(const Error &err) noexcept -> bool;
  [[nodiscard]] AUXID_API auto is_already_held(const Error &err) noexcept -> bool;
} // namespace au::ipc
