#pragma once

#include <iostream>
#include <optional>
#include <variant>

namespace IR {

struct typeless_scalar_t {
  union {
    int32_t i;
    float f;
  } data;
  typeless_scalar_t() {}
  typeless_scalar_t(int32_t i) { data.i = i; }
  typeless_scalar_t(float f) { data.f = f; }
  int32_t &int_value() { return data.i; }
  float &float_value() { return data.f; }
  int32_t int_value() const { return data.i; }
  float float_value() const { return data.f; }
};

typedef std::variant<int32_t, float> typed_scalar_t;

enum class UnaryCompute { LNOT, NEG, ID, FNEG, F2I, I2F, F2D0, F2D1 };

enum class BinaryCompute {
  ADD,
  SUB,
  MUL,
  DIV,
  LESS,
  LEQ,
  EQ,
  NEQ,
  MOD,
  SHL,
  FADD,
  FSUB,
  FMUL,
  FDIV,
  FLESS,
  FLEQ,
  FEQ,
  FNEQ,
};

template <typename GenericScalar>
GenericScalar compute(BinaryCompute, const GenericScalar &lhs,
                      const GenericScalar &rhs);

template <typename GenericScalar>
GenericScalar compute(UnaryCompute, const GenericScalar &a);

inline bool is_useless_compute(UnaryCompute op) {
  if (op == UnaryCompute::ID)
    return true;
  return false;
}

inline bool is_commutable_compute(BinaryCompute op) {
  if (op == BinaryCompute::ADD)
    return true;
  if (op == BinaryCompute::FADD)
    return true;
  if (op == BinaryCompute::MUL)
    return true;
  if (op == BinaryCompute::FMUL)
    return true;
  if (op == BinaryCompute::EQ)
    return true;
  if (op == BinaryCompute::FEQ)
    return true;
  if (op == BinaryCompute::NEQ)
    return true;
  if (op == BinaryCompute::FNEQ)
    return true;
  return false;
}

inline bool is_useless_compute(BinaryCompute op,
                               std::optional<typed_scalar_t> s1,
                               std::optional<typed_scalar_t> s2) {
  if (op == BinaryCompute::ADD) {
    return s1 == typed_scalar_t(0) || s2 == typed_scalar_t(0);
  }
  if (op == BinaryCompute::FADD) {
    // IEEE 754: only `-0.0f` is additive identity
    return s1 == typed_scalar_t(-0.0f) || s2 == typed_scalar_t(-0.0f);
  }
  if (op == BinaryCompute::SUB) {
    return s2 == typed_scalar_t(0);
  }
  if (op == BinaryCompute::FSUB) {
    // <lhs> - 0.0f =  + (-0.0f)
    return s2 == typed_scalar_t(0.0f);
  }
  if (op == BinaryCompute::MUL) {
    return s1 == typed_scalar_t(1) || s2 == typed_scalar_t(1);
  }
  if (op == BinaryCompute::FMUL) {
    return s1 == typed_scalar_t(1.0f) || s2 == typed_scalar_t(1.0f);
  }
  if (op == BinaryCompute::DIV) {
    return s2 == typed_scalar_t(1);
  }
  if (op == BinaryCompute::FDIV) {
    return s2 == typed_scalar_t(1.0f);
  }
  return false;
}

template <typename Scalar>
inline typed_scalar_t typed_compute(UnaryCompute op, const Scalar &s1) {
  union {
    double d;
    float f[2];
  } f2d;
  switch (op) {
  case UnaryCompute::LNOT:
    return int32_t(!s1);
  case UnaryCompute::NEG:
    return -s1;
  case UnaryCompute::ID:
    return s1;
  case UnaryCompute::FNEG:
    return -s1;
  case UnaryCompute::F2I:
    return int32_t(s1);
  case UnaryCompute::I2F:
    return float(s1);
  case UnaryCompute::F2D0:
    f2d.d = s1;
    return f2d.f[0];
  case UnaryCompute::F2D1:
    f2d.d = s1;
    return f2d.f[1];
  default:
    assert(0);
    return 0;
  }
}

template <typename Scalar>
inline typed_scalar_t typed_compute(BinaryCompute op, Scalar s1, Scalar s2) {
  switch (op) {
  case BinaryCompute::ADD:
    return s1 + s2;
  case BinaryCompute::SUB:
    return s1 - s2;
  case BinaryCompute::MUL:
    return s1 * s2;
  case BinaryCompute::DIV:
    return s1 / s2;
  case BinaryCompute::LESS:
    return int32_t(s1 < s2);
  case BinaryCompute::LEQ:
    return int32_t(s1 <= s2);
  case BinaryCompute::EQ:
    return int32_t(s1 == s2);
  case BinaryCompute::NEQ:
    return int32_t(s1 != s2);
  case BinaryCompute::MOD:
    if constexpr (std::is_same<int32_t, Scalar>::value) {
      return s1 % s2;
    } else {
      throw;
    }
  case BinaryCompute::FADD:
    return s1 + s2;
  case BinaryCompute::FSUB:
    return s1 - s2;
  case BinaryCompute::FMUL:
    return s1 * s2;
  case BinaryCompute::FDIV:
    return s1 / s2;
  case BinaryCompute::FLESS:
    return int32_t(s1 < s2);
  case BinaryCompute::FLEQ:
    return int32_t(s1 <= s2);
  case BinaryCompute::FEQ:
    return int32_t(s1 == s2);
  case BinaryCompute::FNEQ:
    return int32_t(s1 != s2);
  default:
    assert(0);
    return 0;
  }
}

template <>
inline typed_scalar_t compute(UnaryCompute op, const typed_scalar_t &s1) {
  if (auto *as_int = std::get_if<int32_t>(&s1))
    return typed_compute(op, *as_int);
  if (auto *as_float = std::get_if<float>(&s1))
    return typed_compute(op, *as_float);
  throw;
}

template <>
inline typed_scalar_t compute(BinaryCompute op, const typed_scalar_t &s1,
                              const typed_scalar_t &s2) {
  if (auto *s1_as_int = std::get_if<int32_t>(&s1))
    if (auto *s2_as_int = std::get_if<int32_t>(&s2))
      return typed_compute(op, *s1_as_int, *s2_as_int);
  if (auto *s1_as_float = std::get_if<float>(&s1))
    if (auto *s2_as_float = std::get_if<float>(&s2))
      return typed_compute(op, *s1_as_float, *s2_as_float);
  throw;
}

template <>
inline typeless_scalar_t compute(BinaryCompute op, const typeless_scalar_t &s1,
                                 const typeless_scalar_t &s2) {
  int32_t i1 = s1.int_value(), i2 = s2.int_value();
  float f1 = s1.float_value(), f2 = s2.float_value();
  switch (op) {
  case BinaryCompute::ADD:
    return i1 + i2;
  case BinaryCompute::SUB:
    return i1 - i2;
  case BinaryCompute::MUL:
    return i1 * i2;
  case BinaryCompute::DIV:
    return (i2 && !(i1 == -2147483648 && i2 == -1) ? i1 / i2 : 0);
  case BinaryCompute::LESS:
    return int32_t(i1 < i2);
  case BinaryCompute::LEQ:
    return int32_t(i1 <= i2);
  case BinaryCompute::EQ:
    return int32_t(i1 == i2);
  case BinaryCompute::NEQ:
    return int32_t(i1 != i2);
  case BinaryCompute::MOD:
    return (i2 ? i1 % i2 : 0);
  case BinaryCompute::SHL:
    return i1 << i2;
  case BinaryCompute::FADD:
    return f1 + f2;
  case BinaryCompute::FSUB:
    return f1 - f2;
  case BinaryCompute::FMUL:
    return f1 * f2;
  case BinaryCompute::FDIV:
    return f1 / f2;
  case BinaryCompute::FLESS:
    return int32_t(f1 < f2);
  case BinaryCompute::FLEQ:
    return int32_t(f1 <= f2);
  case BinaryCompute::FEQ:
    return int32_t(f1 == f2);
  case BinaryCompute::FNEQ:
    return int32_t(f1 != f2);
  default:
    assert(0);
    return 0;
  }
}

template <>
inline typeless_scalar_t compute(UnaryCompute op, const typeless_scalar_t &s1) {
  int32_t i1 = s1.int_value();
  float f1 = s1.float_value();
  union {
    double d;
    float f0, f1;
  } f2d;
  switch (op) {
  case UnaryCompute::LNOT:
    return int32_t(!i1);
  case UnaryCompute::NEG:
    return -i1;
  case UnaryCompute::ID:
    return i1;
  case UnaryCompute::FNEG:
    return -f1;
  case UnaryCompute::F2I:
    return int32_t(f1);
  case UnaryCompute::I2F:
    return float(i1);
  case UnaryCompute::F2D0:
    f2d.d = f1;
    return f2d.f0;
  case UnaryCompute::F2D1:
    f2d.d = f1;
    return f2d.f1;
  default:
    assert(0);
    return 0;
  }
}

} // namespace IR
