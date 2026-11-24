// Auxid: The Rigid C++ Platform.
//
// Copyright (C) 2026 I-A-S (ias@iasoft.dev)
// Copyright (C) 2026 IASoft (PVT) LTD (contact@iasoft.dev)
//
// This source code is licensed under the Apache License, Version 2.0.
// A copy of this license is included in the LICENSE file at the root of this project,
// and is also available at <https://www.apache.org/licenses/LICENSE-2.0>.

import auxid;
import auxid.test;

using namespace au;

namespace
{
  i32 g_destroyed = 0;

  struct Node
  {
    i32 value;
    Arc<Node> next;

    Node(i32 v, Arc<Node> n) : value(v), next(std::move(n))
    {
    }

    ~Node()
    {
      ++g_destroyed;
    }
  };

  // IntrusiveArc<T> needs a complete T, so the next handle sits behind a Box.
  struct IntrusiveLink;

  struct IntrusiveNode : RefCounted
  {
    i32 value;
    Box<IntrusiveLink> link;

    explicit IntrusiveNode(i32 v);
    ~IntrusiveNode();
  };

  struct IntrusiveLink
  {
    IntrusiveArc<IntrusiveNode> next;
  };

  IntrusiveNode::IntrusiveNode(i32 v) : value(v), link(make_box<IntrusiveLink>())
  {
  }

  IntrusiveNode::~IntrusiveNode()
  {
    ++g_destroyed;
  }

  // Assigning from a handle stored inside the block being released:
  // node = node->next, where node is the only owner.
  struct ArcBlock final : test::Block
  {
    [[nodiscard]] auto get_name() const -> const char * override
    {
      return "memory::Arc";
    }

    auto declare_tests() -> void override
    {
      add_test("assign_from_owned_block", [this] { return assign_from_owned_block(); });
      add_test("intrusive_assign_from_owned_block", [this] { return intrusive_assign_from_owned_block(); });
    }

    auto assign_from_owned_block() -> bool
    {
      g_destroyed = 0;
      auto head = make_arc<Node>(1, make_arc<Node>(2, make_arc<Node>(3, Arc<Node>{})));

      head = head->next;
      if (!check_eq(head->value, 2, "copy assignment advances") || !check_eq(g_destroyed, 1, "one node released") ||
          !check_eq(head.use_count(), 1u, "single owner after copy assignment"))
        return false;

      head = std::move(head->next);
      if (!check_eq(head->value, 3, "move assignment advances") || !check_eq(g_destroyed, 2, "two nodes released") ||
          !check_eq(head.use_count(), 1u, "single owner after move assignment"))
        return false;

      head = head->next;
      return check_not(static_cast<bool>(head), "reached the end") && check_eq(g_destroyed, 3, "all nodes released");
    }

    auto intrusive_assign_from_owned_block() -> bool
    {
      g_destroyed = 0;
      auto head = make_intrusive_arc<IntrusiveNode>(1);
      head->link->next = make_intrusive_arc<IntrusiveNode>(2);
      head->link->next->link->next = make_intrusive_arc<IntrusiveNode>(3);

      head = head->link->next;
      if (!check_eq(head->value, 2, "copy assignment advances") || !check_eq(g_destroyed, 1, "one node released") ||
          !check_eq(head.use_count(), 1u, "single owner after copy assignment"))
        return false;

      head = std::move(head->link->next);
      if (!check_eq(head->value, 3, "move assignment advances") || !check_eq(g_destroyed, 2, "two nodes released") ||
          !check_eq(head.use_count(), 1u, "single owner after move assignment"))
        return false;

      head = head->link->next;
      return check_not(static_cast<bool>(head), "reached the end") && check_eq(g_destroyed, 3, "all nodes released");
    }
  };

  const test::AutoRegister<ArcBlock> _registered;
} // namespace
