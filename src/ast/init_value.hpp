#pragma once
#include <cstdint>

#include "ir/ir.hpp"

template <typename Scalar> struct CompileTimeValue {
  Scalar value;
  CompileTimeValue operator-();
  CompileTimeValue operator!();
};

// operators with range checking
template <typename Scalar>
CompileTimeValue<Scalar> operator+(CompileTimeValue<Scalar> lhs,
                                   CompileTimeValue<Scalar> rhs);
template <typename Scalar>
CompileTimeValue<Scalar> operator-(CompileTimeValue<Scalar> lhs,
                                   CompileTimeValue<Scalar> rhs);
template <typename Scalar>
CompileTimeValue<Scalar> operator*(CompileTimeValue<Scalar> lhs,
                                   CompileTimeValue<Scalar> rhs);
template <typename Scalar>
CompileTimeValue<Scalar> operator/(CompileTimeValue<Scalar> lhs,
                                   CompileTimeValue<Scalar> rhs);
template <typename Scalar>
CompileTimeValue<Scalar> operator%(CompileTimeValue<Scalar> lhs,
                                   CompileTimeValue<Scalar> rhs);
template <typename Scalar>
CompileTimeValue<int32_t> operator<(CompileTimeValue<Scalar> lhs,
                                    CompileTimeValue<Scalar> rhs);
template <typename Scalar>
CompileTimeValue<int32_t> operator<=(CompileTimeValue<Scalar> lhs,
                                     CompileTimeValue<Scalar> rhs);
template <typename Scalar>
CompileTimeValue<int32_t> operator==(CompileTimeValue<Scalar> lhs,
                                     CompileTimeValue<Scalar> rhs);
template <typename Scalar>
CompileTimeValue<int32_t> operator!=(CompileTimeValue<Scalar> lhs,
                                     CompileTimeValue<Scalar> rhs);
template <typename Scalar>
CompileTimeValue<int32_t> operator&&(CompileTimeValue<Scalar> lhs,
                                     CompileTimeValue<Scalar> rhs);
template <typename Scalar>
CompileTimeValue<int32_t> operator||(CompileTimeValue<Scalar> lhs,
                                     CompileTimeValue<Scalar> rhs);
