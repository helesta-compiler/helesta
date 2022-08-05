#pragma once

#include <cstring>
#include <string>

#include "arm/block.hpp"
#include "arm/inst.hpp"

namespace ARMv7 {

struct Program;

struct OccurPoint {
  Block *b;
  std::list<std::unique_ptr<Inst>>::iterator it;
  int pos;
  bool operator<(const OccurPoint &y) const {
    if (b != y.b)
      return std::less<Block *>{}(b, y.b);
    else
      return pos < y.pos;
  }
};

struct Func {
  std::string name;
  std::vector<std::unique_ptr<Block>> blocks;
  std::vector<std::unique_ptr<StackObject>> stack_objects,
      caller_stack_object; // caller_stack_object is for argument
  std::vector<Reg> int_arg_reg;
  std::vector<Reg> float_arg_reg;
  Block *entry;
  std::set<Reg> spilling_reg;
  std::map<Reg, int32_t> constant_reg;
  std::map<Reg, std::string> symbol_reg;
  std::map<Reg, std::pair<StackObject *, int32_t>> stack_addr_reg;
  int reg_n;

  std::vector<std::set<OccurPoint>> reg_def, reg_use;
  std::set<Reg> float_regs;

  Func(Program *prog, std::string _name, IR::NormalFunc *ir_func);
  void merge_inst();
  void replace_pseduo_inst();
  void dce();

  void erase_def_use(const OccurPoint &p, Inst *inst);
  void add_def_use(const OccurPoint &p, Inst *inst);
  void build_def_use();
  void calc_live();
  std::vector<int> get_in_deg();
  std::vector<int> get_branch_in_deg();
  void gen_asm(std::ostream &out);
  void print(std::ostream &out);

  inline ScalarType get_reg_type(const int reg_id) const {
    if (float_regs.find(Reg(reg_id, ScalarType::Float)) != float_regs.end()) {
      return ScalarType::Float;
    }
    return ScalarType::Int;
  }

private:
  bool check_store_stack(); // if a StoreStack instruction immediate offset is
                            // out of range, replace with load_imm +
                            // ComplexStore and return false, else return true
  void replace_with_reg_alloc(const std::vector<int> &int_reg_alloc,
                              const std::vector<int> &float_reg_alloc);
  void replace_complex_inst(); // replace out-of-range LoadStack, all of
                               // LoadStackAddr and LoadStackOffset

  typedef std::list<std::unique_ptr<Inst>> List;
  List *_ls;
  List::iterator _it;
  void visit(List &ls, List::iterator it) {
    _it = it;
    _ls = &ls;
  }
  template <class... T> void RegReg(T... args) {
    _ls->insert(_it, std::make_unique<RegRegInst>(args...));
  }
  template <class... T> void RegImm(T... args) {
    _ls->insert(_it, std::make_unique<RegImmInst>(args...));
  }
  void Ins(Inst *x) { _ls->insert(_it, std::unique_ptr<Inst>(x)); }
  void Ins(List &&ls) {
    for (auto &x : ls) {
      _ls->insert(_it, std::move(x));
    }
  }
  void Del() {
    assert(_it != _ls->begin());
    auto p = std::prev(_it);
    *_it = std::move(*p);
    _ls->erase(p);
  }
};

} // namespace ARMv7
