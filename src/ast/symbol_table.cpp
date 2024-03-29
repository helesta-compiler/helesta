#include "ast/symbol_table.hpp"

#include "common/common.hpp"
#include "common/errors.hpp"

using std::string;
using std::unique_ptr;
using std::vector;

Type::Type(ScalarType scalar_type_)
    : scalar_type(scalar_type_), is_const(false), omit_first_dim(false) {
  assert(scalar_type == ScalarType::Int || scalar_type == ScalarType::Float ||
         scalar_type == ScalarType::Char || scalar_type == ScalarType::Void);
}

bool Type::is_array() const { return array_dims.size() > 0 || omit_first_dim; }

Type Type::deref_one_dim() const {
  if (!is_array())
    _throw RuntimeError("Type::deref_one_dim called on a non-array type");
  Type ret = *this;
  if (ret.omit_first_dim) {
    ret.omit_first_dim = false;
  } else {
    ret.array_dims.erase(ret.array_dims.begin());
  }
  return ret;
}

size_t Type::count_array_dims() const {
  return omit_first_dim ? array_dims.size() + 1 : array_dims.size();
}

MemSize Type::count_elements() const {
  if (omit_first_dim)
    _throw RuntimeError("Type::count_elements() called on a not sized type");
  MemSize ret = 1;
  for (MemSize i : array_dims)
    ret *= i;
  return ret;
}

MemSize Type::size() const { return INT_SIZE * count_elements(); }

bool Type::check_assign(const Type &rhs) const {
  if (is_array()) {
    if (!rhs.is_array())
      return false;
    if (scalar_type != rhs.scalar_type)
      return false;
  } else {
    if (rhs.is_array())
      return false;
  }
  if (omit_first_dim) {
    if (rhs.omit_first_dim)
      return array_dims == rhs.array_dims;
    if (rhs.array_dims.size() != array_dims.size() + 1)
      return false;
    for (size_t i = 1; i < rhs.array_dims.size(); ++i)
      if (rhs.array_dims[i] != array_dims[i - 1])
        return false;
    return true;
  } else {
    return (!rhs.omit_first_dim) && array_dims == rhs.array_dims;
  }
}

bool Type::check_index(const vector<MemSize> &index) {
  if (index.size() != count_array_dims())
    return false;
  if (omit_first_dim) {
    for (size_t i = 1; i < index.size(); ++i)
      if (index[i] >= array_dims[i - 1])
        return false;
  } else {
    for (size_t i = 0; i < index.size(); ++i)
      if (index[i] >= array_dims[i])
        return false;
  }
  return true;
}

MemSize Type::get_index(const vector<MemSize> &index) {
  MemSize step = 1, ret = 0;
  size_t next = array_dims.size() - 1;
  for (size_t i = index.size() - 1; i < index.size(); --i) {
    ret += index[i] * step;
    if (next < array_dims.size()) {
      step *= array_dims[next];
      --next;
    }
  }
  return ret;
}

const Type Type::UnknownLengthIntArray = []() {
  Type type(ScalarType::Int);
  type.omit_first_dim = true;
  return type;
}();

const Type Type::UnknownLengthFloatArray = []() {
  Type type(ScalarType::Float);
  type.omit_first_dim = true;
  return type;
}();

std::ostream &operator<<(std::ostream &os, const Type &type) {
  if (type.is_const)
    os << "const ";
  os << type.scalar_type;
  for (auto x : type.array_dims)
    os << "[" << x << "]";
  return os;
}

FunctionInterface::FunctionInterface() : variadic(false) {}

FunctionTableEntry *FunctionTable::resolve(const string &name) {
  auto it = mapping.find(name);
  if (it != mapping.end())
    return it->second.get();
  else
    return nullptr;
}

void FunctionTable::register_func(const string &name, IR::Func *ir_func,
                                  const FunctionInterface &interface) {
  FunctionTableEntry *entry = new FunctionTableEntry();
  entry->ir_func = ir_func;
  entry->interface = interface;
  mapping[name] = unique_ptr<FunctionTableEntry>{entry};
}

VariableTable::VariableTable(VariableTable *_parent) : parent(_parent) {}

VariableTableEntry::VariableTableEntry(ScalarType scalar_type)
    : type(scalar_type),
      const_init(scalar_type == ScalarType::Int
                     ? std::variant<std::vector<int32_t>, std::vector<float>>(
                           std::vector<int32_t>())
                     : std::variant<std::vector<int32_t>, std::vector<float>>(
                           std::vector<float>())) {}

VariableTableEntry *VariableTable::resolve(const string &name) {
  auto it = mapping.find(name);
  if (it != mapping.end())
    return it->second.get();
  else
    return nullptr;
}

VariableTableEntry *VariableTable::recursively_resolve(const string &name) {
  auto it = mapping.find(name);
  if (it != mapping.end())
    return it->second.get();
  if (parent)
    return parent->recursively_resolve(name);
  return nullptr;
}

void VariableTable::register_var(const string &name, IR::MemObject *ir_obj,
                                 const Type &type) {
  VariableTableEntry *entry = new VariableTableEntry(type.scalar_type);
  entry->ir_obj = ir_obj;
  entry->type = type;
  entry->arg_id = -1;
  mapping[name] = unique_ptr<VariableTableEntry>{entry};
}
