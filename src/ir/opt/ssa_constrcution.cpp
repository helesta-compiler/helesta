#include "ir/opt/dom.hpp"
#include "ir/opt/opt.hpp"

void ssa_construction_func(IR::NormalFunc *func) {
  auto dom_ctx = DomTreeBuilderContext(func).construct_dom_tree();
}

void ssa_construction(IR::CompileUnit *ir) {
  ir->for_each(ssa_construction_func);
}
