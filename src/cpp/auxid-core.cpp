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

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string_view>

module auxid.core;

namespace au::detail
{
  namespace
  {
#if AU_PLATFORM_WINDOWS
    // Splits on UTF-8 boundaries so a code point never straddles two chunks; a
    // chunk of N bytes never needs more than N UTF-16 units.
    auto write_console(void *console, std::string_view text) noexcept -> void
    {
      constexpr usize chunk_size = 1024;
      wchar_t wide[chunk_size];

      while (!text.empty())
      {
        usize n = text.size() < chunk_size ? text.size() : chunk_size;
        if (n < text.size())
        {
          usize cut = n;
          while (cut > n - 4 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80)
            --cut;
          if ((static_cast<unsigned char>(text[cut]) & 0xC0) != 0x80)
            n = cut;
        }

        // SAFETY: n <= text.size(), and n bytes never need more than the chunk_size units in wide.
        AU_UNSAFE const int units = ::MultiByteToWideChar(win32::CP_UTF8_, 0, text.data(), static_cast<int>(n), wide,
                                                          static_cast<int>(chunk_size));
        const wchar_t *at = wide;
        unsigned long left = units > 0 ? static_cast<unsigned long>(units) : 0;
        while (left > 0)
        {
          unsigned long written = 0;
          // SAFETY: `at` has `left` converted units behind it.
          AU_UNSAFE const int done = ::WriteConsoleW(console, at, left, &written, nullptr);
          if (done == 0 || written == 0)
            return;
          // SAFETY: written <= left, so `at` stays inside wide or one past the converted units.
          AU_UNSAFE at += written;
          left -= written;
        }
        text.remove_prefix(n);
      }
    }
#endif

    auto write_text(std::FILE *stream, std::string_view text) noexcept -> void
    {
#if AU_PLATFORM_WINDOWS
      // SAFETY: stream is stdout or stderr, see the two callers below.
      AU_UNSAFE const int fd = ::_fileno(stream);
      if (fd >= 0)
      {
        // SAFETY: the CRT returns a HANDLE as an integer; it is only ever passed back to the OS.
        AU_UNSAFE void *handle = reinterpret_cast<void *>(::_get_osfhandle(fd));
        unsigned long mode = 0;
        // SAFETY: the OS rejects a handle that is no console; mode is written only on success.
        AU_UNSAFE const int is_console = ::GetConsoleMode(handle, &mode);
        if (is_console != 0)
        {
          // SAFETY: stream is stdout or stderr.
          AU_UNSAFE std::fflush(stream);
          write_console(handle, text);
          return;
        }
      }
#endif
      // SAFETY: reads exactly text's bytes; stream is stdout or stderr.
      AU_UNSAFE std::fwrite(text.data(), 1, text.size(), stream);
    }
  } // namespace

  AUXID_API auto write_stdout(std::string_view text) noexcept -> void
  {
    write_text(stdout, text);
  }

  AUXID_API auto write_stderr(std::string_view text) noexcept -> void
  {
    write_text(stderr, text);
  }
} // namespace au::detail

namespace au
{
#if !defined(AUXID_DISABLE_DEFAULT_PANIC_HANDLER)
  [[noreturn]] AUXID_API auto panic_handler(const char *msg, const char *file, u32 line) -> void
  {
    char buffer[1024];
    // SAFETY: bounded by sizeof(buffer), and the format matches. file and msg are null-terminated, which every
    // panic entry point requires.
    AU_UNSAFE const int length =
        std::snprintf(buffer, sizeof(buffer), "[PANIC]: (%s:%lu): %s\n", file, static_cast<unsigned long>(line), msg);
    if (length > 0 && static_cast<usize>(length) < sizeof(buffer))
    {
      detail::write_stderr(std::string_view(buffer, static_cast<usize>(length)));
    }
    else
    {
      char line_text[32];
      // SAFETY: bounded by sizeof(line_text), which fits any u32, and the format matches.
      AU_UNSAFE const int digits =
          std::snprintf(line_text, sizeof(line_text), ":%lu): ", static_cast<unsigned long>(line));
      detail::write_stderr("[PANIC]: (");
      detail::write_stderr(file);
      detail::write_stderr(std::string_view(line_text, static_cast<usize>(digits)));
      detail::write_stderr(msg);
      detail::write_stderr("\n");
    }
    std::abort();
  }
#endif

  [[noreturn]] AUXID_API auto panic_at(const char *msg, const char *file, u32 line) -> void
  {
    panic_handler(msg, file, line);
    compiler::trap();
  }

  [[noreturn]] AUXID_API auto panic(const char *msg, std::source_location loc) -> void
  {
    panic_at(msg, loc.file_name(), loc.line());
  }
} // namespace au
