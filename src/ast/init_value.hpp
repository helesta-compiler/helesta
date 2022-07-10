#pragma once
#include <cstdint>

#include "ir/ir.hpp"

#define ALL_UOP uop(!) uop(-)
#define ALL_BOP                                                                \
  bop(+) bop(-) bop(*) bop(/) bop(%) bop(<) bop(<=) bop(==) bop(!=) bop(&&)    \
      bop(||)

template <typename Scalar> struct CompileTimeValue { Scalar value; };

struct CompileTimeValueAny {
private:
  bool is_float;
  union Value {
    int32_t int_value;
    float float_value;
  } _value;

public:
  CompileTimeValueAny() {}
  CompileTimeValueAny(int32_t x) {
    is_float = false;
    _value.int_value = x;
  }
  CompileTimeValueAny(float x) {
    is_float = true;
    _value.float_value = x;
  }
  template <typename Scalar> Scalar value(Scalar *_ = NULL);
  template <typename Scalar> operator CompileTimeValue<Scalar>() {
    return CompileTimeValue<Scalar>{value<Scalar>()};
  }

#define uop(op) friend CompileTimeValueAny operator op(CompileTimeValueAny x);
  ALL_UOP
#undef uop

#define bop(op)                                                                \
  friend CompileTimeValueAny operator op(CompileTimeValueAny x,                \
                                         CompileTimeValueAny y);
  ALL_BOP
#undef bop

  friend CompileTimeValueAny operator>(CompileTimeValueAny x,
                                       CompileTimeValueAny y) {
    return y < x;
  }
  friend CompileTimeValueAny operator>=(CompileTimeValueAny x,
                                        CompileTimeValueAny y) {
    return y <= x;
  }
};

#define uop(op)                                                                \
  template <typename Scalar>                                                   \
  CompileTimeValueAny operator op(CompileTimeValue<Scalar> x);                 \
  CompileTimeValueAny operator op(CompileTimeValueAny x);
ALL_UOP
#undef uop

#define bop(op)                                                                \
  template <typename Scalar>                                                   \
  CompileTimeValueAny operator op(CompileTimeValue<Scalar> lhs,                \
                                  CompileTimeValue<Scalar> rhs);               \
  CompileTimeValueAny operator op(CompileTimeValueAny lhs,                     \
                                  CompileTimeValueAny rhs);
ALL_BOP
#undef bop
