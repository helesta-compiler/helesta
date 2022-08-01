#include "ir/opt/opt.hpp"

void global_to_local(IR::CompileUnit *ir) {
  std::unordered_set<IR::MemObject *> do_not_opt;
  ir->for_each([&](IR::NormalFunc *func) {
    if (func->name == "main")
      return;
    func->for_each([&](IR::Instr *i) {
      if (auto load_addr = dynamic_cast<IR::LoadAddr *>(i)) {
        do_not_opt.insert(load_addr->offset);
      }
    });
  });
  auto main = ir->funcs["main"].get();
  for (auto &mem : ir->scope.objects) {
    if (do_not_opt.find(mem.get()) != do_not_opt.end()) {
      continue;
    }
    if (!mem->is_single_var())
      continue;
    auto raw = mem.release();
    raw->global = false;
    main->scope.add(raw);
  }
  ir->scope.objects.erase(
      std::remove_if(ir->scope.objects.begin(), ir->scope.objects.end(),
                     [&](auto &mem) { return mem == nullptr; }),
      ir->scope.objects.end());
}
