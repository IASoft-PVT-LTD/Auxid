// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

#include <auxid/macros.hpp>

#include <expected>
#include <utility>

import auxid;
import auxid.test;

using namespace au;

namespace
{
  struct CopyTracker
  {
    i32 copies = 0;
    i32 moves = 0;

    CopyTracker() = default;

    CopyTracker(const CopyTracker &o) : copies(o.copies + 1), moves(o.moves)
    {
    }

    CopyTracker(CopyTracker &&o) noexcept : copies(o.copies), moves(o.moves + 1)
    {
    }

    auto operator=(const CopyTracker &o) -> CopyTracker &
    {
      copies = o.copies + 1;
      moves = o.moves;
      return *this;
    }

    auto operator=(CopyTracker &&o) noexcept -> CopyTracker &
    {
      copies = o.copies;
      moves = o.moves + 1;
      return *this;
    }
  };

  auto tracked_inner_fail() -> ResultT<i32, CopyTracker>
  {
    return fail(CopyTracker{});
  }

  auto tracked_outer_propagates() -> ResultT<f32, CopyTracker>
  {
    AU_TRY_VAR(v, tracked_inner_fail());
    return static_cast<f32>(v);
  }

  struct ResultBlock final : test::Block
  {
    [[nodiscard]] auto get_name() const -> const char * override
    {
      return "core::result";
    }

    auto declare_tests() -> void override
    {
      add_test("ok", [this] { return ok(); });
      add_test("err", [this] { return err(); });
      add_test("error_type", [this] { return error_type(); });
      add_test("void_result", [this] { return void_result(); });
      add_test("std_expected_ok", [this] { return std_expected_ok(); });
      add_test("std_expected_err", [this] { return std_expected_err(); });
      add_test("std_expected_void", [this] { return std_expected_void(); });
      add_test("error_propagation_moves", [this] { return error_propagation_moves(); });
      add_test("void_result_copies", [this] { return void_result_copies(); });
    }

    auto error_propagation_moves() -> bool
    {
      // AU_TRY must MOVE the error through every propagation hop; with the
      // default E = String, a copy here is a heap allocation per hop.
      auto res = tracked_outer_propagates();
      if (!check(res.is_err(), "propagated result is_err"))
        return false;
      return check_eq(res.unwrap_err().copies, 0, "error was never copied during propagation") &&
             check(res.unwrap_err().moves >= 1, "error was moved");
    }

    auto void_result_copies() -> bool
    {
      ResultT<void, String> src = fail("void-copy");
      ResultT<void, String> copied = src;
      if (!check(copied.is_err(), "copy-constructed is_err"))
        return false;
      if (!check_eq(copied.unwrap_err(), "void-copy", "copied error matches"))
        return false;

      ResultT<void, String> assigned;
      assigned = src;
      if (!check(assigned.is_err(), "copy-assigned is_err"))
        return false;
      if (!check_eq(assigned.unwrap_err(), "void-copy", "assigned error matches"))
        return false;

      ResultT<void, String> ok_state;
      src = ok_state;
      return check(src.is_ok(), "ok copy-assigned over err");
    }

    auto ok() -> bool
    {
      Result<i32> res = 42;
      return check(res.is_ok(), "res.is_ok()") && check_not(res.is_err(), "!res.is_err()") &&
             check_eq(res.unwrap(), 42, "res.unwrap() == 42");
    }

    auto err() -> bool
    {
      Result<i32> res = fail("Memory allocation failed");
      return check(res.is_err(), "res.is_err()") && check_not(res.is_ok(), "!res.is_ok()") &&
             check_eq(res.unwrap_err().describe(), "Memory allocation failed", "unwrap_err matches");
    }

    auto error_type() -> bool
    {
      Result<i32> res = fail("plain message");
      if (!check(res.is_err(), "fail() is_err"))
        return false;
      const Error &e = res.unwrap_err();
      if (!check(e.domain == ErrorDomain::Generic && e.code == 0, "bare fail() is Generic/0"))
        return false;
      if (!check_eq(e.describe(), "plain message", "Generic describe is just the message"))
        return false;

      if (!check_eq(Error::os(2).describe(), "os/2", "message-free describe is domain/code"))
        return false;

      auto chained = Error(ErrorDomain::Fs, 2, String("open: no such file")).ctx("loading config");
      if (!check(chained.domain == ErrorDomain::Fs && chained.code == 2, "ctx preserves domain and code"))
        return false;
      return check_eq(chained.describe(), "fs/2: loading config: open: no such file",
                      "ctx chains outermost-first");
    }

    auto void_result() -> bool
    {
      ResultT<void, String> res;
      if (!check(res.is_ok(), "default ResultT<void> is_ok"))
        return false;

      ResultT<void, String> errr = fail("Void operation failed");
      return check(errr.is_err(), "fail-constructed is_err") &&
             check_eq(errr.unwrap_err(), "Void operation failed", "unwrap_err matches");
    }

    auto std_expected_ok() -> bool
    {
      // ResultT<T, E> round-trips with std::expected<T, E> for matching E;
      // pinned to String here since the Result<T> default is now Error.
      std::expected<i32, String> from_std{42};
      ResultT<i32, String> ours = from_std;
      if (!check(ours.is_ok(), "ours.is_ok()"))
        return false;
      if (!check_eq(ours.unwrap(), 42, "ours.unwrap() == 42"))
        return false;

      std::expected<i32, String> back_to_std = ours;
      return check(back_to_std.has_value(), "back_to_std.has_value()") &&
             check_eq(*back_to_std, 42, "*back_to_std == 42");
    }

    auto std_expected_err() -> bool
    {
      std::expected<i32, String> err_from_std{std::unexpect, String("boom")};
      ResultT<i32, String> ours = err_from_std;
      if (!check(ours.is_err(), "ours.is_err()"))
        return false;
      if (!check_eq(ours.unwrap_err(), "boom", "ours.unwrap_err() == \"boom\""))
        return false;

      std::expected<i32, String> back_to_std = ours;
      return check_not(back_to_std.has_value(), "!back_to_std.has_value()") &&
             check_eq(back_to_std.error(), "boom", "back_to_std.error() == \"boom\"");
    }

    auto std_expected_void() -> bool
    {
      std::expected<void, String> ok_std;
      ResultT<void, String> ours_ok = ok_std;
      if (!check(ours_ok.is_ok(), "ours_ok.is_ok()"))
        return false;

      std::expected<void, String> err_std{std::unexpect, String("void-fail")};
      ResultT<void, String> ours_err = std::move(err_std);
      if (!check(ours_err.is_err(), "ours_err.is_err()"))
        return false;
      if (!check_eq(ours_err.unwrap_err(), "void-fail", "unwrap_err matches"))
        return false;

      std::expected<void, String> back = ours_err;
      return check_not(back.has_value(), "!back.has_value()") &&
             check_eq(back.error(), "void-fail", "back.error() matches");
    }
  };

  const test::AutoRegister<ResultBlock> _registered;
} // namespace
