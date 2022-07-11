#pragma once

#include "ast/symbol_table.hpp"
#include "ir/ir.hpp"

// when visiting int expression, return IRValue
// when visiting bool expression, return CondJumpList

struct IRValue {
  Type type;
  bool is_left_value;
  IR::Reg reg; // if is_left_value, it's the address instead of the value

  bool assignable() const; // left value, not array and not constant
  IRValue(ScalarType scalar_type);
  friend std::ostream &operator<<(std::ostream &os, const IRValue &x) {
    os << x.type << ' ' << (x.is_left_value ? "lvalue" : "rvalue") << ' '
       << x.reg;
    return os;
  }
};

struct CondJumpList {
  std::vector<IR::BB **> true_list, false_list;
};
