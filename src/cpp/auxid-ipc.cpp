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
#include <string_view>
#include <utility>

#if !AU_PLATFORM_WINDOWS
#  include <fcntl.h>
#  include <sys/file.h>
#  include <sys/socket.h>
#  include <sys/stat.h>
#  include <sys/un.h>
#  include <unistd.h>
#endif

module auxid.ipc;

import auxid.memory;
import auxid.containers;

namespace au::ipc
{
  namespace
  {
#if defined(AU_PLATFORM_WASM)

    [[nodiscard]] auto wasm_absent(const char *op) -> Error
    {
      // Honest absence (contract): no processes, no cross-process IPC.
      return Error(String::format("{}: not expressible in a wasm sandbox", op));
    }

#elif AU_PLATFORM_WINDOWS

    [[nodiscard]] auto to_handle(i64 native) noexcept -> void *
    {
      return reinterpret_cast<void *>(static_cast<std::intptr_t>(native));
    }

    [[nodiscard]] auto from_handle(void *handle) noexcept -> i64
    {
      return static_cast<i64>(reinterpret_cast<std::intptr_t>(handle));
    }

    [[nodiscard]] auto widen(StringView utf8) -> Vec<wchar_t>
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

    // Contract realization: \\.\pipe\auxid.<name>
    [[nodiscard]] auto pipe_name_utf8(StringView name) -> String
    {
      String full("\\\\.\\pipe\\auxid.");
      full.append(name);
      return full;
    }

    [[nodiscard]] auto create_listener_instance(StringView name) -> Result<void *>
    {
      const String full = pipe_name_utf8(name);
      const Vec<wchar_t> wfull = widen(StringView(full.data(), full.size()));
      void *handle = ::CreateNamedPipeW(wfull.data(), win32::PIPE_ACCESS_DUPLEX_,
                                        win32::PIPE_TYPE_BYTE_STREAM_,
                                        win32::PIPE_UNLIMITED_INSTANCES_, 64 * 1024, 64 * 1024, 0,
                                        nullptr);
      if (handle == win32::invalid_handle())
        return fail(Error::os_last().ctx("ipc::ChannelListener::bind"));
      return handle;
    }

    [[nodiscard]] auto read_handle(i64 native, Span<u8> buffer, const char *op) -> Result<usize>
    {
      unsigned long got = 0;
      if (::ReadFile(to_handle(native), buffer.data(), static_cast<unsigned long>(buffer.size()),
                     &got, nullptr) == 0)
      {
        Error err = Error::os_last();
        if (static_cast<unsigned long>(err.code) == win32::ERROR_BROKEN_PIPE_)
          return usize{0}; // peer/writer closed = end of stream
        return fail(std::move(err).ctx(op));
      }
      return static_cast<usize>(got);
    }

    [[nodiscard]] auto write_handle(i64 native, Span<const u8> data, const char *op) -> Result<usize>
    {
      unsigned long put = 0;
      if (::WriteFile(to_handle(native), data.data(), static_cast<unsigned long>(data.size()), &put,
                      nullptr) == 0)
        return fail(Error::os_last().ctx(op));
      return static_cast<usize>(put);
    }

    auto close_handle(i64 &native) -> void
    {
      if (native != K_INVALID_IPC_HANDLE)
      {
        ::CloseHandle(to_handle(native));
        native = K_INVALID_IPC_HANDLE;
      }
    }

#else // POSIX

    // Contract realization: <XDG_RUNTIME_DIR|/tmp>/auxid.<name>.sock|.lock
    [[nodiscard]] auto runtime_path(StringView name, const char *suffix) -> String
    {
      const char *base = ::getenv("XDG_RUNTIME_DIR");
      String path(base != nullptr ? base : "/tmp");
      path.append("/auxid.");
      path.append(name);
      path.append(suffix);
      return path;
    }

    auto set_nosigpipe(int fd) -> void
    {
#  if defined(SO_NOSIGPIPE)
      int one = 1;
      ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#  else
      AU_UNUSED(fd);
#  endif
    }

    [[nodiscard]] auto fill_sockaddr(const String &path, sockaddr_un &addr) -> bool
    {
      if (path.size() + 1 > sizeof(addr.sun_path))
        return false;
      std::memset(&addr, 0, sizeof(addr));
      addr.sun_family = AF_UNIX;
      std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);
      return true;
    }

    [[nodiscard]] auto read_fd(i64 native, Span<u8> buffer, const char *op) -> Result<usize>
    {
      for (;;)
      {
        const auto got = ::read(static_cast<int>(native), buffer.data(), buffer.size());
        if (got >= 0)
          return static_cast<usize>(got); // 0 = end of stream
        if (errno == EINTR)
          continue;
        return fail(Error::os(errno).ctx(op));
      }
    }

    [[nodiscard]] auto write_fd(i64 native, Span<const u8> data, bool is_socket, const char *op)
        -> Result<usize>
    {
      for (;;)
      {
#  if defined(MSG_NOSIGNAL)
        const auto put = is_socket
                             ? ::send(static_cast<int>(native), data.data(), data.size(), MSG_NOSIGNAL)
                             : ::write(static_cast<int>(native), data.data(), data.size());
#  else
        AU_UNUSED(is_socket); // SO_NOSIGPIPE was set at socket creation
        const auto put = ::write(static_cast<int>(native), data.data(), data.size());
#  endif
        if (put >= 0)
          return static_cast<usize>(put);
        if (errno == EINTR)
          continue;
        return fail(Error::os(errno).ctx(op));
      }
    }

    auto close_fd(i64 &native) -> void
    {
      if (native != K_INVALID_IPC_HANDLE)
      {
        ::close(static_cast<int>(native));
        native = K_INVALID_IPC_HANDLE;
      }
    }

#endif
  } // namespace

#if defined(AU_PLATFORM_WASM)

  AUXID_API auto pipe() -> Result<PipePair>
  {
    return fail(wasm_absent("ipc::pipe"));
  }

  AUXID_API auto PipeReader::read(Span<u8>) -> Result<usize>
  {
    return fail(wasm_absent("ipc::PipeReader::read"));
  }

  AUXID_API auto PipeReader::close() -> void
  {
    m_native = K_INVALID_IPC_HANDLE;
  }

  AUXID_API auto PipeWriter::write(Span<const u8>) -> Result<usize>
  {
    return fail(wasm_absent("ipc::PipeWriter::write"));
  }

  AUXID_API auto PipeWriter::close() -> void
  {
    m_native = K_INVALID_IPC_HANDLE;
  }

  AUXID_API auto Channel::connect(StringView) -> Result<Channel>
  {
    return fail(wasm_absent("ipc::Channel::connect"));
  }

  AUXID_API auto Channel::read(Span<u8>) -> Result<usize>
  {
    return fail(wasm_absent("ipc::Channel::read"));
  }

  AUXID_API auto Channel::write(Span<const u8>) -> Result<usize>
  {
    return fail(wasm_absent("ipc::Channel::write"));
  }

  AUXID_API auto Channel::close() -> void
  {
    m_native = K_INVALID_IPC_HANDLE;
  }

  AUXID_API auto ChannelListener::bind(StringView) -> Result<ChannelListener>
  {
    return fail(wasm_absent("ipc::ChannelListener::bind"));
  }

  AUXID_API auto ChannelListener::accept() -> Result<Channel>
  {
    return fail(wasm_absent("ipc::ChannelListener::accept"));
  }

  AUXID_API auto ChannelListener::close() -> void
  {
    m_native = K_INVALID_IPC_HANDLE;
  }

  AUXID_API auto InstanceLock::acquire(StringView) -> Result<InstanceLock>
  {
    return fail(wasm_absent("ipc::InstanceLock::acquire"));
  }

  AUXID_API auto InstanceLock::release() -> void
  {
    m_native = K_INVALID_IPC_HANDLE;
  }

  AUXID_API auto is_closed(const Error &) noexcept -> bool
  {
    return false;
  }

  AUXID_API auto is_already_held(const Error &) noexcept -> bool
  {
    return false;
  }

#elif AU_PLATFORM_WINDOWS

  AUXID_API auto pipe() -> Result<PipePair>
  {
    void *read_end = nullptr;
    void *write_end = nullptr;
    if (::CreatePipe(&read_end, &write_end, nullptr, 0) == 0)
      return fail(Error::os_last().ctx("ipc::pipe"));
    return PipePair{PipeReader::adopt_native(from_handle(read_end)),
                    PipeWriter::adopt_native(from_handle(write_end))};
  }

  AUXID_API auto PipeReader::read(Span<u8> buffer) -> Result<usize>
  {
    return read_handle(m_native, buffer, "ipc::PipeReader::read");
  }

  AUXID_API auto PipeReader::close() -> void
  {
    close_handle(m_native);
  }

  AUXID_API auto PipeWriter::write(Span<const u8> data) -> Result<usize>
  {
    return write_handle(m_native, data, "ipc::PipeWriter::write");
  }

  AUXID_API auto PipeWriter::close() -> void
  {
    close_handle(m_native);
  }

  AUXID_API auto Channel::connect(StringView name) -> Result<Channel>
  {
    const String full = pipe_name_utf8(name);
    const Vec<wchar_t> wfull = widen(StringView(full.data(), full.size()));

    for (int attempt = 0; attempt < 8; attempt++)
    {
      void *handle = ::CreateFileW(wfull.data(), win32::GENERIC_READ_ | win32::GENERIC_WRITE_, 0,
                                   nullptr, win32::OPEN_EXISTING_, 0, nullptr);
      if (handle != win32::invalid_handle())
        return Channel::adopt_native(from_handle(handle));

      Error err = Error::os_last();
      if (static_cast<unsigned long>(err.code) != win32::ERROR_PIPE_BUSY_)
        return fail(std::move(err).ctx("ipc::Channel::connect"));
      // All instances busy: wait for the server to create a fresh one.
      ::WaitNamedPipeW(wfull.data(), 2000);
    }
    return fail("ipc::Channel::connect: pipe stayed busy");
  }

  AUXID_API auto Channel::read(Span<u8> buffer) -> Result<usize>
  {
    return read_handle(m_native, buffer, "ipc::Channel::read");
  }

  AUXID_API auto Channel::write(Span<const u8> data) -> Result<usize>
  {
    return write_handle(m_native, data, "ipc::Channel::write");
  }

  AUXID_API auto Channel::close() -> void
  {
    close_handle(m_native);
  }

  AUXID_API auto ChannelListener::bind(StringView name) -> Result<ChannelListener>
  {
    AU_TRY_VAR(instance, create_listener_instance(name));
    ChannelListener listener;
    listener.m_native = from_handle(instance);
    listener.m_name = String(name);
    return listener;
  }

  AUXID_API auto ChannelListener::accept() -> Result<Channel>
  {
    if (m_native == K_INVALID_IPC_HANDLE)
      return fail("ipc::ChannelListener::accept: listener is closed");

    if (::ConnectNamedPipe(to_handle(m_native), nullptr) == 0)
    {
      Error err = Error::os_last();
      // A client that connected between CreateNamedPipe and ConnectNamedPipe.
      if (static_cast<unsigned long>(err.code) != win32::ERROR_PIPE_CONNECTED_)
        return fail(std::move(err).ctx("ipc::ChannelListener::accept"));
    }

    const i64 connected = m_native;
    m_native = K_INVALID_IPC_HANDLE;

    // Stage the next instance before handing the connected one out.
    auto next = create_listener_instance(StringView(m_name.data(), m_name.size()));
    if (next.is_err())
    {
      ::CloseHandle(to_handle(connected));
      return fail(std::move(next.unwrap_err()));
    }
    m_native = from_handle(next.unwrap());

    return Channel::adopt_native(connected);
  }

  AUXID_API auto ChannelListener::close() -> void
  {
    close_handle(m_native);
  }

  AUXID_API auto InstanceLock::acquire(StringView app_key) -> Result<InstanceLock>
  {
    String full("Local\\auxid.");
    full.append(app_key);
    const Vec<wchar_t> wfull = widen(StringView(full.data(), full.size()));

    void *handle = ::CreateMutexW(nullptr, 1, wfull.data());
    if (handle == nullptr)
      return fail(Error::os_last().ctx("ipc::InstanceLock::acquire"));

    const Error last = Error::os_last();
    if (static_cast<unsigned long>(last.code) == win32::ERROR_ALREADY_EXISTS_)
    {
      ::CloseHandle(handle);
      return fail(Error(ErrorDomain::Os, last.code,
                        String("ipc::InstanceLock::acquire: instance already held")));
    }

    InstanceLock lock;
    lock.m_native = from_handle(handle);
    return lock;
  }

  AUXID_API auto InstanceLock::release() -> void
  {
    close_handle(m_native);
  }

  AUXID_API auto is_closed(const Error &err) noexcept -> bool
  {
    if (err.domain != ErrorDomain::Os)
      return false;
    const auto code = static_cast<unsigned long>(err.code);
    return code == win32::ERROR_BROKEN_PIPE_ || code == win32::ERROR_NO_DATA_;
  }

  AUXID_API auto is_already_held(const Error &err) noexcept -> bool
  {
    return err.domain == ErrorDomain::Os &&
           static_cast<unsigned long>(err.code) == win32::ERROR_ALREADY_EXISTS_;
  }

#else // POSIX

  AUXID_API auto pipe() -> Result<PipePair>
  {
    int fds[2] = {-1, -1};
    if (::pipe(fds) != 0)
      return fail(Error::os(errno).ctx("ipc::pipe"));
    return PipePair{PipeReader::adopt_native(fds[0]), PipeWriter::adopt_native(fds[1])};
  }

  AUXID_API auto PipeReader::read(Span<u8> buffer) -> Result<usize>
  {
    return read_fd(m_native, buffer, "ipc::PipeReader::read");
  }

  AUXID_API auto PipeReader::close() -> void
  {
    close_fd(m_native);
  }

  AUXID_API auto PipeWriter::write(Span<const u8> data) -> Result<usize>
  {
    return write_fd(m_native, data, false, "ipc::PipeWriter::write");
  }

  AUXID_API auto PipeWriter::close() -> void
  {
    close_fd(m_native);
  }

  AUXID_API auto Channel::connect(StringView name) -> Result<Channel>
  {
    const String path = runtime_path(name, ".sock");
    sockaddr_un addr{};
    if (!fill_sockaddr(path, addr))
      return fail("ipc::Channel::connect: socket path too long");

    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
      return fail(Error::os(errno).ctx("ipc::Channel::connect"));
    set_nosigpipe(fd);

    if (::connect(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) != 0)
    {
      const int saved = errno;
      ::close(fd);
      return fail(Error::os(saved).ctx("ipc::Channel::connect"));
    }
    return Channel::adopt_native(fd);
  }

  AUXID_API auto Channel::read(Span<u8> buffer) -> Result<usize>
  {
    return read_fd(m_native, buffer, "ipc::Channel::read");
  }

  AUXID_API auto Channel::write(Span<const u8> data) -> Result<usize>
  {
    return write_fd(m_native, data, true, "ipc::Channel::write");
  }

  AUXID_API auto Channel::close() -> void
  {
    close_fd(m_native);
  }

  AUXID_API auto ChannelListener::bind(StringView name) -> Result<ChannelListener>
  {
    const String path = runtime_path(name, ".sock");
    sockaddr_un addr{};
    if (!fill_sockaddr(path, addr))
      return fail("ipc::ChannelListener::bind: socket path too long");

    // Stale-socket recovery (contract): probe; a refused connection means a
    // dead owner, so the stale node is unlinked before binding.
    {
      const int probe = ::socket(AF_UNIX, SOCK_STREAM, 0);
      if (probe >= 0)
      {
        if (::connect(probe, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) == 0)
        {
          ::close(probe);
          return fail(Error(ErrorDomain::Os, EADDRINUSE,
                            String("ipc::ChannelListener::bind: name already in use")));
        }
        ::close(probe);
        ::unlink(path.c_str());
      }
    }

    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
      return fail(Error::os(errno).ctx("ipc::ChannelListener::bind"));
    set_nosigpipe(fd);

    if (::bind(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) != 0 ||
        ::listen(fd, 8) != 0)
    {
      const int saved = errno;
      ::close(fd);
      return fail(Error::os(saved).ctx("ipc::ChannelListener::bind"));
    }
    ::chmod(path.c_str(), 0600); // owner-only (contract)

    ChannelListener listener;
    listener.m_native = fd;
    listener.m_name = path; // realized path, for unlink on close
    return listener;
  }

  AUXID_API auto ChannelListener::accept() -> Result<Channel>
  {
    if (m_native == K_INVALID_IPC_HANDLE)
      return fail("ipc::ChannelListener::accept: listener is closed");

    for (;;)
    {
      const int fd = ::accept(static_cast<int>(m_native), nullptr, nullptr);
      if (fd >= 0)
      {
        set_nosigpipe(fd);
        return Channel::adopt_native(fd);
      }
      if (errno == EINTR)
        continue;
      return fail(Error::os(errno).ctx("ipc::ChannelListener::accept"));
    }
  }

  AUXID_API auto ChannelListener::close() -> void
  {
    if (m_native != K_INVALID_IPC_HANDLE && !m_name.empty())
      ::unlink(m_name.c_str());
    close_fd(m_native);
  }

  AUXID_API auto InstanceLock::acquire(StringView app_key) -> Result<InstanceLock>
  {
    const String path = runtime_path(app_key, ".lock");
    const int fd = ::open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (fd < 0)
      return fail(Error::os(errno).ctx("ipc::InstanceLock::acquire"));

    if (::flock(fd, LOCK_EX | LOCK_NB) != 0)
    {
      const int saved = errno;
      ::close(fd);
      return fail(
          Error(ErrorDomain::Os, saved, String("ipc::InstanceLock::acquire: instance already held")));
    }

    // The lock file is deliberately never unlinked (unlink-vs-flock races);
    // the flock is released when the descriptor closes.
    InstanceLock lock;
    lock.m_native = fd;
    lock.m_path = path;
    return lock;
  }

  AUXID_API auto InstanceLock::release() -> void
  {
    close_fd(m_native);
  }

  AUXID_API auto is_closed(const Error &err) noexcept -> bool
  {
    return err.domain == ErrorDomain::Os && (err.code == EPIPE || err.code == ECONNRESET);
  }

  AUXID_API auto is_already_held(const Error &err) noexcept -> bool
  {
    return err.domain == ErrorDomain::Os &&
           (err.code == EWOULDBLOCK || err.code == EAGAIN || err.code == EACCES);
  }

#endif

  // Shared across platforms.

  AUXID_API auto PipeWriter::write_all(Span<const u8> data) -> Result<void>
  {
    usize done = 0;
    while (done < data.size())
    {
      AU_TRY_VAR(put, write(data.subspan(done)));
      if (put == 0)
        return fail("ipc::PipeWriter::write_all: zero-byte write");
      done += put;
    }
    return {};
  }

  AUXID_API auto Channel::write_all(Span<const u8> data) -> Result<void>
  {
    usize done = 0;
    while (done < data.size())
    {
      AU_TRY_VAR(put, write(data.subspan(done)));
      if (put == 0)
        return fail("ipc::Channel::write_all: zero-byte write");
      done += put;
    }
    return {};
  }
} // namespace au::ipc
