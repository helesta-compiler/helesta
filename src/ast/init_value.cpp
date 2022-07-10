#include "init_value.hpp"

#include <climits>

#include "ast/symbol_table.hpp"
#include "common/common.hpp"
#include "common/errors.hpp"

#define check_and_return(x)                                                    \
  if ((x) < INT32_MIN || (x) > INT32_MAX)                                      \
    throw CompileTimeValueEvalFail("compile-time value out of bound");         \
  return static_cast<int32_t>(x);

// do not check for float by now

template <> CompileTimeValueAny operator-(CompileTimeValue<float> x) {
  return -x.value;
}

template <> CompileTimeValueAny operator!(CompileTimeValue<float> x) {
  return static_cast<int32_t>(!x.value);
}

template <>
CompileTimeValueAny operator+(CompileTimeValue<float> lhs,
                              CompileTimeValue<float> rhs) {
  return lhs.value + rhs.value;
}

template <>
CompileTimeValueAny operator-(CompileTimeValue<float> lhs,
                              CompileTimeValue<float> rhs) {
  return lhs.value - rhs.value;
}

template <>
CompileTimeValueAny operator*(CompileTimeValue<float> lhs,
                              CompileTimeValue<float> rhs) {
  return lhs.value * rhs.value;
}

template <>
CompileTimeValueAny operator/(CompileTimeValue<float> lhs,
                              CompileTimeValue<float> rhs) {
  return lhs.value / rhs.value;
}

template <>
CompileTimeValueAny operator%(CompileTimeValue<float> lhs,
                              CompileTimeValue<float> rhs) {
  throw CompileTimeValueEvalFail(
      "float mod in compile-time constant evaluation");
}

template <> CompileTimeValueAny operator-(CompileTimeValue<int32_t> x) {
  if (x.value == INT_MIN)
    return INT_MIN;
  int64_t res = -static_cast<int64_t>(x.value);
  check_and_return(res)
}

template <> CompileTimeValueAny operator!(CompileTimeValue<int32_t> x) {
  return static_cast<int32_t>(!x.value);
}

template <>
CompileTimeValueAny operator+(CompileTimeValue<int32_t> lhs,
                              CompileTimeValue<int32_t> rhs) {
  int64_t res =
      static_cast<int64_t>(lhs.value) + static_cast<int64_t>(rhs.value);
  check_and_return(res)
}

template <>
CompileTimeValueAny operator-(CompileTimeValue<int32_t> lhs,
                              CompileTimeValue<int32_t> rhs) {
  int64_t res =
      static_cast<int64_t>(lhs.value) - static_cast<int64_t>(rhs.value);
  check_and_return(res)
}

template <>
CompileTimeValueAny operator*(CompileTimeValue<int32_t> lhs,
                              CompileTimeValue<int32_t> rhs) {
  int64_t res =
      static_cast<int64_t>(lhs.value) * static_cast<int64_t>(rhs.value);
  check_and_return(res)
}

template <>
CompileTimeValueAny operator/(CompileTimeValue<int32_t> lhs,
                              CompileTimeValue<int32_t> rhs) {
  if (rhs.value == 0)
    throw CompileTimeValueEvalFail(
        "division by zero in compile-time constant evaluation");
  int64_t res =
      static_cast<int64_t>(lhs.value) / static_cast<int64_t>(rhs.value);
  check_and_return(res)
}

template <>
CompileTimeValueAny operator%(CompileTimeValue<int32_t> lhs,
                              CompileTimeValue<int32_t> rhs) {
  if (rhs.value == 0)
    throw CompileTimeValueEvalFail(
        "division by zero in compile-time constant evaluation");
  int64_t res =
      static_cast<int64_t>(lhs.value) % static_cast<int64_t>(rhs.value);
  check_and_return(res)
}

#define bop(op)                                                                \
  template <>                                                                  \
  CompileTimeValueAny operator op(CompileTimeValue<int32_t> lhs,               \
                                  CompileTimeValue<int32_t> rhs) {             \
    return static_cast<int32_t>(lhs.value op rhs.value);                       \
  }                                                                            \
  template <>                                                                  \
  CompileTimeValueAny operator op(CompileTimeValue<float> lhs,                 \
                                  CompileTimeValue<float> rhs) {               \
    return static_cast<int32_t>(lhs.value op rhs.value);                       \
  }

bop(<) bop(<=) bop(==) bop(!=) bop(&&) bop(||)

#undef bop

    template <>
    float CompileTimeValueAny::value<float>(float *) {
  if (is_float) {
    return _value.float_value;
  }
  return _value.int_value;
}
template <> int32_t CompileTimeValueAny::value<int32_t>(int32_t *) {
  if (is_float) {
    return _value.float_value;
  }
  return _value.int_value;
}

#define uop(op)                                                                \
  CompileTimeValueAny operator op(CompileTimeValueAny x) {                     \
    if (x.is_float) {                                                          \
      return op CompileTimeValue<float>{x.value<float>()};                     \
    }                                                                          \
    return op CompileTimeValue<int32_t>{x.value<int32_t>()};                   \
  }
ALL_UOP
#undef uop

#define bop(op)                                                                \
  CompileTimeValueAny operator op(CompileTimeValueAny lhs,                     \
                                  CompileTimeValueAny rhs) {                   \
    if (lhs.is_float || rhs.is_float) {                                        \
      return CompileTimeValue<float>{                                          \
          lhs.value<float>()} op CompileTimeValue<float>{rhs.value<float>()};  \
    }                                                                          \
    return CompileTimeValue<int32_t>{                                          \
        lhs.value<int32_t>()} op CompileTimeValue<int32_t>{                    \
        rhs.value<int32_t>()};                                                 \
  }
ALL_BOP
#undef bop