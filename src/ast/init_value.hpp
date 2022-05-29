#pragma once
#include <cstdint>

#include "ir/ir.hpp"

struct CompileTimeValue {
  int32_t value;
  CompileTimeValue operator-();
  CompileTimeValue operator!();
};

// operators with range checking
CompileTimeValue operator+(CompileTimeValue lhs, CompileTimeValue rhs);
CompileTimeValue operator-(CompileTimeValue lhs, CompileTimeValue rhs);
CompileTimeValue operator*(CompileTimeValue lhs, CompileTimeValue rhs);
CompileTimeValue operator/(CompileTimeValue lhs, CompileTimeValue rhs);
CompileTimeValue operator%(CompileTimeValue lhs, CompileTimeValue rhs);
CompileTimeValue operator<(CompileTimeValue lhs, CompileTimeValue rhs);
CompileTimeValue operator<=(CompileTimeValue lhs, CompileTimeValue rhs);
CompileTimeValue operator==(CompileTimeValue lhs, CompileTimeValue rhs);
CompileTimeValue operator!=(CompileTimeValue lhs, CompileTimeValue rhs);
CompileTimeValue operator&&(CompileTimeValue lhs, CompileTimeValue rhs);
CompileTimeValue operator||(CompileTimeValue lhs, CompileTimeValue rhs);
