#include "ast/symbol_table.hpp"

#include "common/common.hpp"
#include "common/errors.hpp"

using std::string;
using std::unique_ptr;
using std::vector;

template<typename ScalarType>
GenericType<ScalarType>::GenericType() : is_const(false), omit_first_dim(false) {}

template<typename ScalarType>
bool GenericType<ScalarType>::is_array() const { return array_dims.size() > 0 || omit_first_dim; }

template<typename ScalarType>
GenericType<ScalarType> GenericType<ScalarType>::deref_one_dim() const {
  if (!is_array())
    throw RuntimeError("GenericType::deref_one_dim called on a non-array type");
  GenericType ret = *this;
  if (ret.omit_first_dim) {
    ret.omit_first_dim = false;
  } else {
    ret.array_dims.erase(ret.array_dims.begin());
  }
  return ret;
}

template<typename ScalarType>
size_t GenericType<ScalarType>::count_array_dims() const {
  return omit_first_dim ? array_dims.size() + 1 : array_dims.size();
}

template<typename ScalarType>
MemSize GenericType<ScalarType>::count_elements() const {
  if (omit_first_dim)
    throw RuntimeError("GenericType::count_elements() called on a not sized type");
  MemSize ret = 1;
  for (MemSize i : array_dims)
    ret *= i;
  return ret;
}

template<typename ScalarType>
MemSize GenericType<ScalarType>::size() const { return INT_SIZE * count_elements(); }

template<typename ScalarType>
bool GenericType<ScalarType>::check_assign(const GenericType &rhs) const {
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

template<typename ScalarType>
bool GenericType<ScalarType>::check_index(const vector<MemSize> &index) {
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

template<typename ScalarType>
MemSize GenericType<ScalarType>::get_index(const vector<MemSize> &index) {
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


template<typename ScalarType>
const GenericType<ScalarType> GenericType<ScalarType>::UnknownLengthArray = []() {
  GenericType type;
  type.omit_first_dim = true;
  return type;
}();

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

template<typename ScalarType>
void VariableTable::register_var(const string &name, IR::MemObject *ir_obj,
                                 const GenericType<ScalarType> &type) {
  VariableTableEntry *entry = new VariableTableEntry(type.scalar_type);
  entry->ir_obj = ir_obj;
  entry->arg_id = -1;
  VTInfo<ScalarType> info;
  info.gType = type;
  entry->info = info;
  mapping[name] = unique_ptr<VariableTableEntry>{entry};
}

template <typename ScalarType>
void VariableTable::register_const(const string &name, IR::MemObject *ir_obj,
                                   const GenericType<ScalarType> &type,
                                   std::vector<ScalarType> init) {
  VariableTableEntry *entry = new VariableTableEntry(type.scalar_type);
  entry->ir_obj = ir_obj;
  entry->arg_id = -1;
  VTInfo<ScalarType> info;
  info.gType = type;
  info.const_init = std::move(init);
  entry->info = info;
  mapping[name] = unique_ptr<VariableTableEntry>{entry};
}
