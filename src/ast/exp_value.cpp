#include "ast/exp_value.hpp"

IRValue::IRValue(ScalarType scalar_type) : type(Type(scalar_type)) {}

bool IRValue::assignable() const {
  return is_left_value && (!type.is_const) && (!type.is_array());
}
