#pragma once

#include "ast/symbol_table.hpp"
#include "ir/ir.hpp"

// when visiting int expression, return IRValue
// when visiting bool expression, return CondJumpList

struct IRValue {
  Type type;
  bool is_left_value;
  IR::Reg reg; // if is_left_value, it's the address instead of the value

  IRValue() : type(Type(ScalarType::int_t)) {}

  bool assignable() const; // left value, not array and not constant
};

struct CondJumpList {
  std::vector<IR::BB **> true_list, false_list;
};
