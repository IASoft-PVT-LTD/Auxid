// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

// The ONLY sanctioned source of Win32/CRT declarations for Auxid module
// units. <windows.h> must never appear in a module unit's global fragment:
// mingw's winnt.h drags in the x86 intrinsics headers, which collide with
// the same headers snapshotted inside auxid.containers under GCC's modules
// implementation ("conflicting language linkage for imported declaration").
// Signatures below are canonical; 64-bit Windows has a single calling
// convention, so the bare prototypes are exact. Add entries here, never
// include <windows.h>.

#pragma once

#include <auxid/macros.hpp>

#if AU_PLATFORM_WINDOWS

extern "C"
{
  // kernel32 — errors
  unsigned long __stdcall GetLastError(void);

  // kernel32 — handles & threads
  int __stdcall CloseHandle(void *handle);
  unsigned long __stdcall WaitForSingleObject(void *handle, unsigned long milliseconds);
  unsigned long __stdcall GetCurrentThreadId(void);

  // CRT — thread spawn
  unsigned long long __cdecl _beginthreadex(void *security, unsigned stack_size,
                                            unsigned(__stdcall *start_address)(void *), void *arglist,
                                            unsigned initflag, unsigned *thrdaddr);

  // kernel32 — UTF-8 <-> UTF-16
  int __stdcall MultiByteToWideChar(unsigned int code_page, unsigned long flags, const char *multi_byte,
                                    int multi_byte_len, wchar_t *wide, int wide_len);
  int __stdcall WideCharToMultiByte(unsigned int code_page, unsigned long flags, const wchar_t *wide,
                                    int wide_len, char *multi_byte, int multi_byte_len,
                                    const char *default_char, int *used_default_char);

  // kernel32 — environment & module paths
  unsigned long __stdcall GetEnvironmentVariableW(const wchar_t *name, wchar_t *buffer,
                                                  unsigned long buffer_size);
  int __stdcall SetEnvironmentVariableW(const wchar_t *name, const wchar_t *value);
  unsigned long __stdcall GetModuleFileNameW(void *module, wchar_t *filename, unsigned long size);

  // kernel32 — anonymous pipes & file I/O (security attributes passed as
  // void*; Auxid only ever passes nullptr)
  int __stdcall CreatePipe(void **read_pipe, void **write_pipe, void *security_attributes,
                           unsigned long size);
  int __stdcall ReadFile(void *handle, void *buffer, unsigned long to_read, unsigned long *read,
                         void *overlapped);
  int __stdcall WriteFile(void *handle, const void *buffer, unsigned long to_write,
                          unsigned long *written, void *overlapped);
  int __stdcall FlushFileBuffers(void *handle);

  // kernel32 — named pipes
  void *__stdcall CreateNamedPipeW(const wchar_t *name, unsigned long open_mode,
                                   unsigned long pipe_mode, unsigned long max_instances,
                                   unsigned long out_buffer_size, unsigned long in_buffer_size,
                                   unsigned long default_timeout, void *security_attributes);
  int __stdcall ConnectNamedPipe(void *handle, void *overlapped);
  int __stdcall DisconnectNamedPipe(void *handle);
  void *__stdcall CreateFileW(const wchar_t *name, unsigned long desired_access,
                              unsigned long share_mode, void *security_attributes,
                              unsigned long creation_disposition, unsigned long flags,
                              void *template_file);
  int __stdcall WaitNamedPipeW(const wchar_t *name, unsigned long timeout_ms);

  // kernel32 — named mutex (single-instance arbitration)
  void *__stdcall CreateMutexW(void *security_attributes, int initial_owner, const wchar_t *name);

  // kernel32 — process creation & supervision. Struct layouts are declared
  // below in au::win32 (layout-identical to the SDK's) and passed as void*.
  int __stdcall CreateProcessW(const wchar_t *application_name, wchar_t *command_line,
                               void *process_attributes, void *thread_attributes,
                               int inherit_handles, unsigned long creation_flags,
                               void *environment, const wchar_t *current_directory,
                               void *startup_info, void *process_information);
  void *__stdcall GetStdHandle(unsigned long std_handle);
  int __stdcall SetHandleInformation(void *handle, unsigned long mask, unsigned long flags);
  int __stdcall GetExitCodeProcess(void *process, unsigned long *exit_code);
  int __stdcall TerminateProcess(void *process, unsigned int exit_code);
  unsigned long __stdcall ResumeThread(void *thread);
  void *__stdcall CreateJobObjectW(void *security_attributes, const wchar_t *name);
  int __stdcall SetInformationJobObject(void *job, int information_class, void *information,
                                        unsigned long length);
  int __stdcall AssignProcessToJobObject(void *job, void *process);

  // kernel32 — dynamic libraries
  void *__stdcall LoadLibraryW(const wchar_t *path);
  void *__stdcall GetProcAddress(void *module, const char *name); // FARPROC, ABI-compatible
  int __stdcall FreeLibrary(void *module);
}

namespace au::win32
{
  inline constexpr unsigned int CP_UTF8_ = 65001;
  inline constexpr unsigned long ERROR_ENVVAR_NOT_FOUND_ = 203;
  inline constexpr unsigned long ERROR_INSUFFICIENT_BUFFER_ = 122;
  inline constexpr unsigned long ERROR_ALREADY_EXISTS_ = 183;
  inline constexpr unsigned long ERROR_BROKEN_PIPE_ = 109;
  inline constexpr unsigned long ERROR_NO_DATA_ = 232;
  inline constexpr unsigned long ERROR_PIPE_BUSY_ = 231;
  inline constexpr unsigned long ERROR_PIPE_CONNECTED_ = 535;
  inline constexpr unsigned long GENERIC_READ_ = 0x80000000ul;
  inline constexpr unsigned long GENERIC_WRITE_ = 0x40000000ul;
  inline constexpr unsigned long OPEN_EXISTING_ = 3;
  inline constexpr unsigned long PIPE_ACCESS_DUPLEX_ = 0x00000003ul;
  inline constexpr unsigned long PIPE_TYPE_BYTE_STREAM_ = 0x00000000ul; // TYPE_BYTE|READMODE_BYTE|WAIT
  inline constexpr unsigned long PIPE_UNLIMITED_INSTANCES_ = 255;

  inline void *invalid_handle() noexcept
  {
    return reinterpret_cast<void *>(-1ll);
  }

  // Process / job constants
  inline constexpr unsigned long STD_INPUT_HANDLE_ = static_cast<unsigned long>(-10);
  inline constexpr unsigned long STD_OUTPUT_HANDLE_ = static_cast<unsigned long>(-11);
  inline constexpr unsigned long STD_ERROR_HANDLE_ = static_cast<unsigned long>(-12);
  inline constexpr unsigned long HANDLE_FLAG_INHERIT_ = 0x1;
  inline constexpr unsigned long STARTF_USESTDHANDLES_ = 0x100;
  inline constexpr unsigned long CREATE_SUSPENDED_ = 0x4;
  inline constexpr unsigned long CREATE_UNICODE_ENVIRONMENT_ = 0x400;
  inline constexpr unsigned long FILE_SHARE_READ_WRITE_ = 0x3;
  inline constexpr unsigned long WAIT_OBJECT_0_ = 0;
  inline constexpr unsigned long WAIT_TIMEOUT_ = 258;
  inline constexpr int JOB_OBJECT_EXTENDED_LIMIT_INFORMATION_ = 9;
  inline constexpr unsigned long JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE_ = 0x2000;

  // Layout-identical SDK structs (64-bit), passed to the prototypes as void*.
  struct StartupInfoW
  {
    unsigned long cb;
    wchar_t *reserved;
    wchar_t *desktop;
    wchar_t *title;
    unsigned long x, y, x_size, y_size, x_count_chars, y_count_chars, fill_attribute, flags;
    unsigned short show_window, cb_reserved2;
    unsigned char *reserved2;
    void *std_input, *std_output, *std_error;
  };
  static_assert(sizeof(StartupInfoW) == 104, "STARTUPINFOW layout drift");

  struct ProcessInformation
  {
    void *process;
    void *thread;
    unsigned long process_id;
    unsigned long thread_id;
  };
  static_assert(sizeof(ProcessInformation) == 24, "PROCESS_INFORMATION layout drift");

  struct JobBasicLimits
  {
    long long per_process_user_time_limit;
    long long per_job_user_time_limit;
    unsigned long limit_flags;
    unsigned long long minimum_working_set_size;
    unsigned long long maximum_working_set_size;
    unsigned long active_process_limit;
    unsigned long long affinity;
    unsigned long priority_class;
    unsigned long scheduling_class;
  };

  struct JobIoCounters
  {
    unsigned long long read_operation_count, write_operation_count, other_operation_count;
    unsigned long long read_transfer_count, write_transfer_count, other_transfer_count;
  };

  struct JobExtendedLimits
  {
    JobBasicLimits basic;
    JobIoCounters io;
    unsigned long long process_memory_limit;
    unsigned long long job_memory_limit;
    unsigned long long peak_process_memory_used;
    unsigned long long peak_job_memory_used;
  };
  static_assert(sizeof(JobExtendedLimits) == 144, "JOBOBJECT_EXTENDED_LIMIT_INFORMATION layout drift");
} // namespace au::win32

#endif // AU_PLATFORM_WINDOWS
