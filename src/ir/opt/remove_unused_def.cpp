#include "ir/opt/opt.hpp"

std::vector<IR::Instr *> construct_def_vec(IR::NormalFunc *func) {
  std::vector<IR::Instr *> def_vec(func->max_reg_id + 1, nullptr);
  func->for_each([&](IR::Instr *i) {
    if (auto reg_write_instr = dynamic_cast<IR::RegWriteInstr *>(i)) {
      assert(def_vec[reg_write_instr->d1.id] == nullptr);
      def_vec[reg_write_instr->d1.id] = reg_write_instr;
    }
  });
  return def_vec;
}

void remove_unused_def_func(IR::NormalFunc *func) {
  auto def_vec = construct_def_vec(func);
  std::unordered_set<IR::Instr *> used_instrs;
  std::deque<IR::Instr *> q;
  func->for_each([&](IR::Instr *i) {
    if (auto call_instr = dynamic_cast<IR::CallInstr *>(i)) {
      if (!call_instr->pure)
        q.push_back(i);
    } else if (dynamic_cast<IR::ControlInstr *>(i))
      q.push_back(i);
    else if (dynamic_cast<IR::StoreInstr *>(i))
      q.push_back(i);
    else if (dynamic_cast<IR::LocalVarDef *>(i))
      q.push_back(i);
  });
  while (!q.empty()) {
    auto i = q.front();
    q.pop_front();
    if (used_instrs.find(i) != used_instrs.end())
      continue;
    used_instrs.insert(i);
    i->map_use([&](IR::Reg &r) {
      assert(def_vec[r.id] != nullptr);
      q.push_back(def_vec[r.id]);
    });
  }
  func->for_each([&](IR::BB *bb) {
    bb->instrs.remove_if([&](std::unique_ptr<IR::Instr> &i) {
      return used_instrs.find(i.get()) == used_instrs.end();
    });
  });
}

void remove_unused_bb(IR::NormalFunc *func) {
  std::unordered_set<IR::BB *> used_bb;
  std::deque<IR::BB *> queue;
  used_bb.insert(func->entry);
  queue.push_back(func->entry);
  while (!queue.empty()) {
    auto b = queue.front();
    queue.pop_front();
    if (dynamic_cast<IR::ControlInstr *>(b->back())) {
      std::vector<IR::BB *> out_nodes = b->getOutNodes();
      for (auto target : out_nodes) {
        if (used_bb.find(target) != used_bb.end()) {
          continue;
        }
        used_bb.insert(target);
        queue.push_back(target);
      }
    } else {
      int next_id = b->id + 1;
      for (auto it = func->bbs.begin(); it != func->bbs.end(); ++it) {
        IR::BB *tmp_bb = (*it).get();
        if (tmp_bb->id == next_id) {
          if (used_bb.find(tmp_bb) == used_bb.end()) {
            used_bb.insert(tmp_bb);
            queue.push_back(tmp_bb);
          }
          break;
        }
      }
    }
  }
  for (auto it = func->bbs.begin(); it != func->bbs.end();) {
    if (used_bb.find((*it).get()) == used_bb.end()) {
      it = func->bbs.erase(it);
    } else {
      ++it;
    }
  }
}

void remove_unused_def(IR::CompileUnit *ir) {
  ir->for_each(remove_unused_def_func);
  ir->for_each(remove_unused_bb);
}
