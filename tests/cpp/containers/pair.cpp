// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

#include <utility>

import auxid;
import auxid.test;

using namespace au;

namespace
{
  struct PairBlock final : test::Block
  {
    [[nodiscard]] auto get_name() const -> const char * override
    {
      return "containers::pair";
    }

    auto declare_tests() -> void override
    {
      add_test("construction", [this] { return construction(); });
      add_test("move_semantics", [this] { return move_semantics(); });
    }

    auto construction() -> bool
    {
      Pair<i32, String> p(42, "Answer");
      return check_eq(p.first, 42, "p.first == 42") && check_eq(p.second, "Answer", "p.second == \"Answer\"");
    }

    auto move_semantics() -> bool
    {
      Pair<i32, String> p1(100, "Moving");
      Pair<i32, String> p2(std::move(p1));
      return check_eq(p2.first, 100, "p2.first == 100") && check_eq(p2.second, "Moving", "p2.second == \"Moving\"");
    }
  };

  const test::AutoRegister<PairBlock> _registered;
} // namespace
