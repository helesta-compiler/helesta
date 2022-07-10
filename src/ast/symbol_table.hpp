#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "ast/init_value.hpp"
#include "common/common.hpp"
#include "ir/ir.hpp"

struct Type {
  ScalarType scalar_type;
  std::vector<MemSize> array_dims;
  bool is_const, omit_first_dim;

  Type() = delete;
  Type(ScalarType scalar_type_);
  bool is_array() const;      // array_dims not empty || omit_first_dim
  Type deref_one_dim() const; // throw when not array. return the type where
                              // the first dimension is removed
  size_t count_array_dims() const;
  MemSize count_elements() const;
  MemSize size() const;
  bool check_assign(const Type &rhs) const; // don't check is_const
  bool check_index(const std::vector<MemSize> &index);
  MemSize
  get_index(const std::vector<MemSize> &index); // call when check_index is true

  static const Type UnknownLengthIntArray;
  static const Type UnknownLengthFloatArray;
};

struct StringType {};

struct FunctionInterface {
  bool variadic;
  ScalarType return_type;
  std::vector<std::variant<Type, StringType>> args_type;

  FunctionInterface();
};

struct FunctionTableEntry {
  IR::Func *ir_func;
  FunctionInterface interface;
};

struct FunctionTable {
  std::map<std::string, std::unique_ptr<FunctionTableEntry>> mapping;

  FunctionTableEntry *resolve(const std::string &name);
  void register_func(const std::string &name, IR::Func *ir_func,
                     const FunctionInterface &interface);
};

struct VariableTableEntry {
  IR::MemObject *ir_obj;
  Type type;
  int arg_id; // -1 if not array parameter
  std::variant<std::vector<int32_t>, std::vector<float>>
      const_init; // empty when !type.is_const
  VariableTableEntry(ScalarType);
};

struct VariableTable {
  std::map<std::string, std::unique_ptr<VariableTableEntry>> mapping;
  VariableTable *parent;

  VariableTable(VariableTable *_parent);
  VariableTableEntry *resolve(const std::string &name);
  VariableTableEntry *recursively_resolve(const std::string &name);
  void register_var(const std::string &name, IR::MemObject *ir_obj,
                    const Type &type);
  template <typename ScalarType>
  void register_const(const std::string &name, IR::MemObject *ir_obj,
                      const Type &type, std::vector<ScalarType> init) {

    VariableTableEntry *entry = new VariableTableEntry(type.scalar_type);
    entry->ir_obj = ir_obj;
    entry->type = type;
    entry->arg_id = -1;
    entry->const_init = std::move(init);
    mapping[name] = std::unique_ptr<VariableTableEntry>{entry};
  }
};
