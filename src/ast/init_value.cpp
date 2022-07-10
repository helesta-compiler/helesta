#include "init_value.hpp"

#include <climits>

#include "ast/symbol_table.hpp"
#include "common/common.hpp"
#include "common/errors.hpp"

#define check_and_return(x)                                                    \
  if ((x) < INT32_MIN || (x) > INT32_MAX)                                      \
    throw CompileTimeValueEvalFail("compile-time value out of bound");         \
  return CompileTimeValue<int32_t>{static_cast<int32_t>(x)};

// do not check for float by now

template <> CompileTimeValue<float> CompileTimeValue<float>::operator-() {
  return CompileTimeValue<float>{-value};
}

template <> CompileTimeValue<float> CompileTimeValue<float>::operator!() {
  return CompileTimeValue<float>{static_cast<float>(!value)};
}

template <>
CompileTimeValue<float> operator+(CompileTimeValue<float> lhs,
                                  CompileTimeValue<float> rhs) {
  return CompileTimeValue<float>{lhs.value + rhs.value};
}

template <>
CompileTimeValue<float> operator-(CompileTimeValue<float> lhs,
                                  CompileTimeValue<float> rhs) {
  return CompileTimeValue<float>{lhs.value - rhs.value};
}

template <>
CompileTimeValue<float> operator*(CompileTimeValue<float> lhs,
                                  CompileTimeValue<float> rhs) {
  return CompileTimeValue<float>{lhs.value * rhs.value};
}

template <>
CompileTimeValue<float> operator/(CompileTimeValue<float> lhs,
                                  CompileTimeValue<float> rhs) {
  return CompileTimeValue<float>{lhs.value / rhs.value};
}

template <> CompileTimeValue<int32_t> CompileTimeValue<int32_t>::operator-() {
  if (value == INT_MIN)
    return CompileTimeValue{INT_MIN};
  int64_t res = -static_cast<int64_t>(value);
  check_and_return(res)
}

template <> CompileTimeValue<int32_t> CompileTimeValue<int32_t>::operator!() {
  return CompileTimeValue{!value};
}

template <>
CompileTimeValue<int32_t> operator+(CompileTimeValue<int32_t> lhs,
                                    CompileTimeValue<int32_t> rhs) {
  int64_t res =
      static_cast<int64_t>(lhs.value) + static_cast<int64_t>(rhs.value);
  check_and_return(res)
}

template <>
CompileTimeValue<int32_t> operator-(CompileTimeValue<int32_t> lhs,
                                    CompileTimeValue<int32_t> rhs) {
  int64_t res =
      static_cast<int64_t>(lhs.value) - static_cast<int64_t>(rhs.value);
  check_and_return(res)
}

template <>
CompileTimeValue<int32_t> operator*(CompileTimeValue<int32_t> lhs,
                                    CompileTimeValue<int32_t> rhs) {
  int64_t res =
      static_cast<int64_t>(lhs.value) * static_cast<int64_t>(rhs.value);
  check_and_return(res)
}

template <>
CompileTimeValue<int32_t> operator/(CompileTimeValue<int32_t> lhs,
                                    CompileTimeValue<int32_t> rhs) {
  if (rhs.value == 0)
    throw CompileTimeValueEvalFail(
        "division by zero in compile-time constant evaluation");
  int64_t res =
      static_cast<int64_t>(lhs.value) / static_cast<int64_t>(rhs.value);
  check_and_return(res)
}

template <>
CompileTimeValue<int32_t> operator%(CompileTimeValue<int32_t> lhs,
                                    CompileTimeValue<int32_t> rhs) {
  if (rhs.value == 0)
    throw CompileTimeValueEvalFail(
        "division by zero in compile-time constant evaluation");
  int64_t res =
      static_cast<int64_t>(lhs.value) % static_cast<int64_t>(rhs.value);
  check_and_return(res)
}

template <>
CompileTimeValue<int32_t> operator<(CompileTimeValue<int32_t> lhs,
                                    CompileTimeValue<int32_t> rhs) {
  return CompileTimeValue<int32_t>{lhs.value < rhs.value};
}

template <>
CompileTimeValue<int32_t> operator<=(CompileTimeValue<int32_t> lhs,
                                     CompileTimeValue<int32_t> rhs) {
  return CompileTimeValue<int32_t>{lhs.value <= rhs.value};
}

template <>
CompileTimeValue<int32_t> operator==(CompileTimeValue<int32_t> lhs,
                                     CompileTimeValue<int32_t> rhs) {
  return CompileTimeValue<int32_t>{lhs.value == rhs.value};
}

template <>
CompileTimeValue<int32_t> operator!=(CompileTimeValue<int32_t> lhs,
                                     CompileTimeValue<int32_t> rhs) {
  return CompileTimeValue<int32_t>{lhs.value != rhs.value};
}

template <>
CompileTimeValue<int32_t> operator&&(CompileTimeValue<int32_t> lhs,
                                     CompileTimeValue<int32_t> rhs) {
  return CompileTimeValue<int32_t>{lhs.value && rhs.value};
}

template <>
CompileTimeValue<int32_t> operator||(CompileTimeValue<int32_t> lhs,
                                     CompileTimeValue<int32_t> rhs) {
  return CompileTimeValue<int32_t>{lhs.value || rhs.value};
}

template <>
CompileTimeValue<int32_t> operator<(CompileTimeValue<float> lhs,
                                    CompileTimeValue<float> rhs) {
  return CompileTimeValue<int32_t>{lhs.value < rhs.value};
}

template <>
CompileTimeValue<int32_t> operator<=(CompileTimeValue<float> lhs,
                                     CompileTimeValue<float> rhs) {
  return CompileTimeValue<int32_t>{lhs.value <= rhs.value};
}

template <>
CompileTimeValue<int32_t> operator==(CompileTimeValue<float> lhs,
                                     CompileTimeValue<float> rhs) {
  return CompileTimeValue<int32_t>{lhs.value == rhs.value};
}

template <>
CompileTimeValue<int32_t> operator!=(CompileTimeValue<float> lhs,
                                     CompileTimeValue<float> rhs) {
  return CompileTimeValue<int32_t>{lhs.value != rhs.value};
}

template <>
CompileTimeValue<int32_t> operator&&(CompileTimeValue<float> lhs,
                                     CompileTimeValue<float> rhs) {
  return CompileTimeValue<int32_t>{lhs.value && rhs.value};
}

template <>
CompileTimeValue<int32_t> operator||(CompileTimeValue<float> lhs,
                                     CompileTimeValue<float> rhs) {
  return CompileTimeValue<int32_t>{lhs.value || rhs.value};
}
