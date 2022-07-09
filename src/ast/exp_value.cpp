#include "ast/exp_value.hpp"

template <typename ScalarType> bool IRValue<ScalarType>::assignable() const {
  return is_left_value && (!type.is_const) && (!type.is_array());
}
