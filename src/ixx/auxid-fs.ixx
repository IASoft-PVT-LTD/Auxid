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

#include <cstdint>
#include <filesystem>
#include <system_error>
#include <utility>

export module auxid.fs;

export import auxid.core;
import auxid.containers;

export namespace au::filesystem
{
  namespace fs = std::filesystem;

  using CopyOptions = fs::copy_options;
  using DirectoryOptions = fs::directory_options;
  using FileStatus = fs::file_status;
  using FileTimeType = fs::file_time_type;
  using FileType = fs::file_type;
  using Path = fs::path;
  using Perms = fs::perms;
  using PermOptions = fs::perm_options;
  using SpaceInfo = fs::space_info;

  using DirectoryEntry = fs::directory_entry;
  using DirectoryIterator = fs::directory_iterator;
  using RecursiveDirectoryIterator = fs::recursive_directory_iterator;

  using fs::hash_value;
  using fs::status_known;
} // namespace au::filesystem

namespace au::filesystem::_internal
{
  [[nodiscard]] inline auto fail_fs(const char *op, const std::error_code &ec)
  {
    // Fs domain, raw platform error value preserved (D-008): errno-family on
    // POSIX, Win32 codes on Windows — callers can dispatch on ec fidelity
    // instead of parsing prose.
    return au::fail(Error(ErrorDomain::Fs, ec.value(), String::format("{}: {}", op, ec.message())));
  }

  [[nodiscard]] inline auto absolute_relative_to_base(const Path &p, const Path &base) -> Result<Path>
  {
    std::error_code ec;
    Path base_leaf = fs::weakly_canonical(base, ec);
    if (ec)
      return fail_fs("absolute", ec);
    Path ret = fs::weakly_canonical(base_leaf / p, ec);
    if (ec)
      return fail_fs("absolute", ec);
    return ret;
  }

  inline void directory_iterator_advance(DirectoryIterator &it, std::error_code &ec)
  {
    it.increment(ec);
  }

  inline void recursive_directory_iterator_advance(RecursiveDirectoryIterator &it, std::error_code &ec)
  {
    it.increment(ec);
  }
} // namespace au::filesystem::_internal

export namespace au::filesystem
{
  [[nodiscard]] inline auto absolute(const Path &p) -> Result<Path>
  {
    std::error_code ec;
    Path out = fs::absolute(p, ec);
    if (ec)
      return _internal::fail_fs("absolute", ec);
    return out;
  }

  [[nodiscard]] inline auto absolute(const Path &p, const Path &base) -> Result<Path>
  {
    return _internal::absolute_relative_to_base(p, base);
  }

  [[nodiscard]] inline auto canonical(const Path &p) -> Result<Path>
  {
    std::error_code ec;
    Path out = fs::canonical(p, ec);
    if (ec)
      return _internal::fail_fs("canonical", ec);
    return out;
  }

  [[nodiscard]] inline auto weakly_canonical(const Path &p) -> Result<Path>
  {
    std::error_code ec;
    Path out = fs::weakly_canonical(p, ec);
    if (ec)
      return _internal::fail_fs("weakly_canonical", ec);
    return out;
  }

  [[nodiscard]] inline auto relative(const Path &p) -> Result<Path>
  {
    std::error_code ec;
    Path out = fs::relative(p, ec);
    if (ec)
      return _internal::fail_fs("relative", ec);
    return out;
  }

  [[nodiscard]] inline auto relative(const Path &p, const Path &base) -> Result<Path>
  {
    std::error_code ec;
    Path out = fs::relative(p, base, ec);
    if (ec)
      return _internal::fail_fs("relative", ec);
    return out;
  }

  [[nodiscard]] inline auto proximate(const Path &p) -> Result<Path>
  {
    std::error_code ec;
    Path out = fs::proximate(p, ec);
    if (ec)
      return _internal::fail_fs("proximate", ec);
    return out;
  }

  [[nodiscard]] inline auto proximate(const Path &p, const Path &base) -> Result<Path>
  {
    std::error_code ec;
    Path out = fs::proximate(p, base, ec);
    if (ec)
      return _internal::fail_fs("proximate", ec);
    return out;
  }

  [[nodiscard]] inline auto read_symlink(const Path &p) -> Result<Path>
  {
    std::error_code ec;
    Path out = fs::read_symlink(p, ec);
    if (ec)
      return _internal::fail_fs("read_symlink", ec);
    return out;
  }

  [[nodiscard]] inline auto current_path() -> Result<Path>
  {
    std::error_code ec;
    Path out = fs::current_path(ec);
    if (ec)
      return _internal::fail_fs("current_path", ec);
    return out;
  }

  inline auto current_path(const Path &p) -> Result<void>
  {
    std::error_code ec;
    fs::current_path(p, ec);
    if (ec)
      return _internal::fail_fs("current_path", ec);
    return Result<void>();
  }

  [[nodiscard]] inline auto temp_directory_path() -> Result<Path>
  {
    std::error_code ec;
    Path out = fs::temp_directory_path(ec);
    if (ec)
      return _internal::fail_fs("temp_directory_path", ec);
    return out;
  }

  [[nodiscard]] inline auto status(const Path &p) -> Result<FileStatus>
  {
    std::error_code ec;
    FileStatus st = fs::status(p, ec);
    if (ec)
      return _internal::fail_fs("status", ec);
    return st;
  }

  [[nodiscard]] inline auto symlink_status(const Path &p) -> Result<FileStatus>
  {
    std::error_code ec;
    FileStatus st = fs::symlink_status(p, ec);
    if (ec)
      return _internal::fail_fs("symlink_status", ec);
    return st;
  }

  [[nodiscard]] inline auto exists(const Path &p) -> Result<bool>
  {
    std::error_code ec;
    bool ok = fs::exists(p, ec);
    if (ec)
      return _internal::fail_fs("exists", ec);
    return ok;
  }

  [[nodiscard]] inline auto equivalent(const Path &p1, const Path &p2) -> Result<bool>
  {
    std::error_code ec;
    bool ok = fs::equivalent(p1, p2, ec);
    if (ec)
      return _internal::fail_fs("equivalent", ec);
    return ok;
  }

  [[nodiscard]] inline auto is_block_file(const Path &p) -> Result<bool>
  {
    std::error_code ec;
    bool ok = fs::is_block_file(p, ec);
    if (ec)
      return _internal::fail_fs("is_block_file", ec);
    return ok;
  }

  [[nodiscard]] inline auto is_character_file(const Path &p) -> Result<bool>
  {
    std::error_code ec;
    bool ok = fs::is_character_file(p, ec);
    if (ec)
      return _internal::fail_fs("is_character_file", ec);
    return ok;
  }

  [[nodiscard]] inline auto is_directory(const Path &p) -> Result<bool>
  {
    std::error_code ec;
    bool ok = fs::is_directory(p, ec);
    if (ec)
      return _internal::fail_fs("is_directory", ec);
    return ok;
  }

  [[nodiscard]] inline auto is_empty(const Path &p) -> Result<bool>
  {
    std::error_code ec;
    bool ok = fs::is_empty(p, ec);
    if (ec)
      return _internal::fail_fs("is_empty", ec);
    return ok;
  }

  [[nodiscard]] inline auto is_fifo(const Path &p) -> Result<bool>
  {
    std::error_code ec;
    bool ok = fs::is_fifo(p, ec);
    if (ec)
      return _internal::fail_fs("is_fifo", ec);
    return ok;
  }

  [[nodiscard]] inline auto is_other(const Path &p) -> Result<bool>
  {
    std::error_code ec;
    bool ok = fs::is_other(p, ec);
    if (ec)
      return _internal::fail_fs("is_other", ec);
    return ok;
  }

  [[nodiscard]] inline auto is_regular_file(const Path &p) -> Result<bool>
  {
    std::error_code ec;
    bool ok = fs::is_regular_file(p, ec);
    if (ec)
      return _internal::fail_fs("is_regular_file", ec);
    return ok;
  }

  [[nodiscard]] inline auto is_socket(const Path &p) -> Result<bool>
  {
    std::error_code ec;
    bool ok = fs::is_socket(p, ec);
    if (ec)
      return _internal::fail_fs("is_socket", ec);
    return ok;
  }

  [[nodiscard]] inline auto is_symlink(const Path &p) -> Result<bool>
  {
    std::error_code ec;
    bool ok = fs::is_symlink(p, ec);
    if (ec)
      return _internal::fail_fs("is_symlink", ec);
    return ok;
  }

  [[nodiscard]] inline auto file_size(const Path &p) -> Result<std::uintmax_t>
  {
    std::error_code ec;
    std::uintmax_t n = fs::file_size(p, ec);
    if (ec)
      return _internal::fail_fs("file_size", ec);
    return n;
  }

  [[nodiscard]] inline auto hard_link_count(const Path &p) -> Result<std::uintmax_t>
  {
    std::error_code ec;
    std::uintmax_t n = fs::hard_link_count(p, ec);
    if (ec)
      return _internal::fail_fs("hard_link_count", ec);
    return n;
  }

  [[nodiscard]] inline auto last_write_time(const Path &p) -> Result<FileTimeType>
  {
    std::error_code ec;
    FileTimeType t = fs::last_write_time(p, ec);
    if (ec)
      return _internal::fail_fs("last_write_time", ec);
    return t;
  }

  inline auto last_write_time(const Path &p, FileTimeType new_time) -> Result<void>
  {
    std::error_code ec;
    fs::last_write_time(p, new_time, ec);
    if (ec)
      return _internal::fail_fs("last_write_time", ec);
    return Result<void>();
  }

  [[nodiscard]] inline auto space(const Path &p) -> Result<SpaceInfo>
  {
    std::error_code ec;
    SpaceInfo s = fs::space(p, ec);
    if (ec)
      return _internal::fail_fs("space", ec);
    return s;
  }

  inline auto copy(const Path &from, const Path &to, CopyOptions options = CopyOptions::none) -> Result<void>
  {
    std::error_code ec;
    fs::copy(from, to, options, ec);
    if (ec)
      return _internal::fail_fs("copy", ec);
    return Result<void>();
  }

  inline auto copy_file(const Path &from, const Path &to, CopyOptions options = CopyOptions::none) -> Result<void>
  {
    std::error_code ec;
    fs::copy_file(from, to, options, ec);
    if (ec)
      return _internal::fail_fs("copy_file", ec);
    return Result<void>();
  }

  inline auto copy_symlink(const Path &from, const Path &to) -> Result<void>
  {
    std::error_code ec;
    fs::copy_symlink(from, to, ec);
    if (ec)
      return _internal::fail_fs("copy_symlink", ec);
    return Result<void>();
  }

  [[nodiscard]] inline auto create_directory(const Path &p) -> Result<bool>
  {
    std::error_code ec;
    bool created = fs::create_directory(p, ec);
    if (ec)
      return _internal::fail_fs("create_directory", ec);
    return created;
  }

  [[nodiscard]] inline auto create_directory(const Path &p, const Path &attributes) -> Result<bool>
  {
    std::error_code ec;
    bool created = fs::create_directory(p, attributes, ec);
    if (ec)
      return _internal::fail_fs("create_directory", ec);
    return created;
  }

  [[nodiscard]] inline auto create_directories(const Path &p) -> Result<bool>
  {
    std::error_code ec;
    bool created = fs::create_directories(p, ec);
    if (ec)
      return _internal::fail_fs("create_directories", ec);
    return created;
  }

  inline auto create_hard_link(const Path &target, const Path &link) -> Result<void>
  {
    std::error_code ec;
    fs::create_hard_link(target, link, ec);
    if (ec)
      return _internal::fail_fs("create_hard_link", ec);
    return Result<void>();
  }

  inline auto create_symlink(const Path &target, const Path &link) -> Result<void>
  {
    std::error_code ec;
    fs::create_symlink(target, link, ec);
    if (ec)
      return _internal::fail_fs("create_symlink", ec);
    return Result<void>();
  }

  inline auto create_directory_symlink(const Path &target, const Path &link) -> Result<void>
  {
    std::error_code ec;
    fs::create_directory_symlink(target, link, ec);
    if (ec)
      return _internal::fail_fs("create_directory_symlink", ec);
    return Result<void>();
  }

  inline auto permissions(const Path &p, Perms prms, PermOptions opts = PermOptions::replace) -> Result<void>
  {
    std::error_code ec;
    fs::permissions(p, prms, opts, ec);
    if (ec)
      return _internal::fail_fs("permissions", ec);
    return Result<void>();
  }

  [[nodiscard]] inline auto remove(const Path &p) -> Result<bool>
  {
    std::error_code ec;
    bool removed = fs::remove(p, ec);
    if (ec)
      return _internal::fail_fs("remove", ec);
    return removed;
  }

  [[nodiscard]] inline auto remove_all(const Path &p) -> Result<std::uintmax_t>
  {
    std::error_code ec;
    std::uintmax_t n = fs::remove_all(p, ec);
    if (ec)
      return _internal::fail_fs("remove_all", ec);
    return n;
  }

  inline auto rename(const Path &from, const Path &to) -> Result<void>
  {
    std::error_code ec;
    fs::rename(from, to, ec);
    if (ec)
      return _internal::fail_fs("rename", ec);
    return Result<void>();
  }

  inline auto resize_file(const Path &p, std::uintmax_t size) -> Result<void>
  {
    std::error_code ec;
    fs::resize_file(p, size, ec);
    if (ec)
      return _internal::fail_fs("resize_file", ec);
    return Result<void>();
  }

  [[nodiscard]] inline auto make_directory_iterator(const Path &p, DirectoryOptions options = DirectoryOptions::none)
      -> Result<DirectoryIterator>
  {
    std::error_code ec;
    DirectoryIterator it(p, options, ec);
    if (ec)
      return _internal::fail_fs("DirectoryIterator", ec);
    return std::move(it);
  }

  inline auto advance(DirectoryIterator &it) -> Result<void>
  {
    std::error_code ec;
    _internal::directory_iterator_advance(it, ec);
    if (ec)
      return _internal::fail_fs("DirectoryIterator::increment", ec);
    return Result<void>();
  }

  [[nodiscard]] inline auto make_recursive_directory_iterator(const Path &p,
                                                              DirectoryOptions options = DirectoryOptions::none)
      -> Result<RecursiveDirectoryIterator>
  {
    std::error_code ec;
    RecursiveDirectoryIterator it(p, options, ec);
    if (ec)
      return _internal::fail_fs("recursive_directory_iterator", ec);
    return std::move(it);
  }

  inline auto advance(RecursiveDirectoryIterator &it) -> Result<void>
  {
    std::error_code ec;
    _internal::recursive_directory_iterator_advance(it, ec);
    if (ec)
      return _internal::fail_fs("recursive_directory_iterator::increment", ec);
    return Result<void>();
  }

  inline auto pop(RecursiveDirectoryIterator &it) -> Result<void>
  {
    std::error_code ec;
    it.pop(ec);
    if (ec)
      return _internal::fail_fs("recursive_directory_iterator::pop", ec);
    return Result<void>();
  }
} // namespace au::filesystem
