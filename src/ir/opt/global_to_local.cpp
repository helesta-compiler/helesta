#include "ir/ir.hpp"
#include "ir/opt/opt.hpp"

void global_to_local(IR::CompileUnit *ir) {
  std::unordered_set<IR::MemObject *> do_not_opt;
  ir->for_each([&](IR::NormalFunc *func) {
    if (func->name == "main" || func->name == ".init")
      return;
    func->for_each([&](IR::Instr *i) {
      if (auto load_addr = dynamic_cast<IR::LoadAddr *>(i)) {
        do_not_opt.insert(load_addr->offset);
      }
    });
  });
  auto main = ir->funcs["main"].get();
  std::unordered_map<IR::MemObject *, IR::MemObject *> global2local;
  for (auto &mem : ir->scope.objects) {
    if (do_not_opt.find(mem.get()) != do_not_opt.end()) {
      continue;
    }
    if (!mem->is_single_var())
      continue;
    auto local = main->scope.new_MemObject(mem->name + "::to_local");
    local->size = 4;
    local->scalar_type = mem->scalar_type;
    global2local[mem.get()] = local;
    auto global_addr = main->new_Reg();
    auto global_val = main->new_Reg();
    auto local_addr = main->new_Reg();
    main->entry->push_front(new IR::StoreInstr(local_addr, global_val));
    main->entry->push_front(new IR::LoadAddr(local_addr, local));
    main->entry->push_front(new IR::LoadInstr(global_val, global_addr));
    main->entry->push_front(new IR::LoadAddr(global_addr, mem.get()));
  }
  main->for_each([&](IR::Instr *i) {
    if (auto load_addr = dynamic_cast<IR::LoadAddr *>(i)) {
      if (global2local.find(load_addr->offset) != global2local.end()) {
        load_addr->offset = global2local[load_addr->offset];
      }
    }
  });
  for (auto &mem : ir->scope.objects) {
    if (do_not_opt.find(mem.get()) != do_not_opt.end()) {
      continue;
    }
    if (!mem->is_single_var())
      continue;
    auto local = global2local[mem.get()];
    auto global_addr = main->new_Reg();
    auto global_val = main->new_Reg();
    auto local_addr = main->new_Reg();
    main->entry->push_front(new IR::StoreInstr(local_addr, global_val));
    main->entry->push_front(new IR::LoadAddr(local_addr, local));
    main->entry->push_front(new IR::LoadInstr(global_val, global_addr));
    main->entry->push_front(new IR::LoadAddr(global_addr, mem.get()));
  }
}
