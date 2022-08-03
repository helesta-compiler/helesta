#include "ir/opt/dag_ir.hpp"

void DAG_IR::rev_traverse(BB *w) {
  auto &wi = loop_tree[w];
  if (!wi.reachable || wi.returnable)
    return;
  wi.returnable = 1;
  for (auto u : wi.in)
    rev_traverse(u);
}
void DAG_IR::traverse(BB *w) {
  auto &wi = loop_tree[w];
  if (wi.reachable)
    return;
  wi.reachable = 1;
  ++reach_cnt;
  // std::cerr << "reach" << $(w) << std::endl;
  for (auto u : wi.out)
    traverse(u);
}
void DAG_IR::find_loop(BB *w, BB *head) {
  if (w == head)
    return;
  auto &wi = loop_tree[w];
  if (!wi.reachable)
    return;
  if (wi.visited) {
    find_loop(wi.loop_head, head);
  } else {
    wi.visited = 1;
    wi.loop_head = head;
    // std::cerr << "loop : " << $(head) << " : " << $(w) << '\n';
    loop_tree[head].loop_ch.push_back(w);
    for (auto x : wi.in)
      find_loop(x, head);
  }
}
void DAG_IR::_build_dag_dfn(BB *w, BB *head, BB *prev) {
  auto &wi = loop_tree[w];
  if (wi.dfn_visited)
    return;
  // std::cerr << "build_dag_dfn : " << $(w) << " : " << $(head) << '\n';
  if (w != head && wi.loop_head != head) {
    loop_tree[head].loop_exit.push_back(w);
    loop_tree[head].may_exit.push_back(prev);
    assert(prev);
    return;
  }
  wi.dfn_visited = 1;
  if (w != head && !wi.is_loop_head) {
    wi.loop_exit = wi.out;
    wi.may_exit.push_back(w);
    wi.dfn.push_back(w);
  }
  for (auto u : w == head ? wi.out : wi.loop_exit)
    _build_dag_dfn(u, head, w);
  loop_tree[head].dfn.push_back(w);
}
void DAG_IR::build_dag_dfn(BB *w, BB *head) {
  _build_dag_dfn(w, head);
  auto &hi = loop_tree[head];
  std::reverse(hi.dfn.begin(), hi.dfn.end());
  hi.dfn_visited = 0;
}
void DAG_IR::print(BB *head) {
  auto &hi = loop_tree[head];
  std::cerr << $(head) << "  " << hi.is_loop_head;
  std::cerr << "  dfn: ";
  for (auto u : hi.dfn) {
    std::cerr << $(u) << " ";
  }
  std::cerr << "  exit: ";
  for (auto u : hi.loop_exit) {
    std::cerr << $(u) << " ";
  }
  std::cerr << "  may_exit: ";
  for (auto u : hi.may_exit) {
    std::cerr << $(u) << " ";
  }
  std::cerr << '\n';
}
void DAG_IR::check_dag(BB *head) {
  ++check_cnt;
  // std::cerr << "check" << $(head) << std::endl;
  auto &hi = loop_tree[head];
  if (head != nullptr)
    assert(hi.dfn.at(0) == head);
  for (auto u : hi.dfn) {
    if (u != head)
      check_dag(u);
  }
  size_t id = 0;
  std::map<BB *, size_t> idfn;
  for (auto u : hi.dfn) {
    idfn[u] = id++;
  }
  std::set<BB *> exit(hi.loop_exit.begin(), hi.loop_exit.end());
  std::set<BB *> may_exit(hi.may_exit.begin(), hi.may_exit.end());
  size_t exit_cnt = 0;
  for (auto u : hi.dfn) {
    assert(idfn.count(u));
    auto &ui = loop_tree[u];
    for (auto v : u == head ? ui.out : ui.loop_exit) {
      if (v == head) {
        hi.back_edge.push_back(u);
      } else if (idfn.count(v)) {
        assert(idfn[u] < idfn[v]);
        assert(loop_tree[v].loop_head == head);
      } else {
        // std::cerr << "exit edge : " << $(head) << " : " << $(u) << " : "
        //          << $(v) << '\n';
        assert(may_exit.count(u));
        assert(exit.count(v));
        assert(loop_tree[v].loop_head != head);
        ++exit_cnt;
      }
    }
  }
  assert(!hi.back_edge.empty() == hi.is_loop_head);
  assert(exit_cnt == hi.loop_exit.size());
  hi.loop_exit = std::vector<BB *>(exit.begin(), exit.end());
  hi.may_exit = std::vector<BB *>(may_exit.begin(), may_exit.end());
}
DAG_IR::DAG_IR(NormalFunc *_func) : func(_func) {
  // std::cerr << func->name << std::endl;
  dom = construct_dom_tree(func);
  for (auto dom_x : dom->dfn) {
    auto x = dom_x->bb;
    loop_tree[x].dom_tree_node = dom_x;
  }
  for (auto dom_x : dom->dfn) {
    auto x = dom_x->bb;
    auto &xi = loop_tree[x];
    xi.out = x->getOutNodes();
  }
  traverse(func->entry);
  for (auto dom_x : dom->dfn) {
    auto x = dom_x->bb;
    auto &xi = loop_tree[x];
    if (!xi.reachable)
      continue;
    // std::cerr << $(x) << '\n';
    for (auto y : xi.out) {
      // std::cerr << $(x) << " -> " << $(y) << '\n';
      loop_tree[y].in.push_back(x);
    }
  }
  func->for_each([&](BB *bb) {
    Case(ReturnInstr, _, bb->back()) {
      (void)_;
      rev_traverse(bb);
    }
  });

  size_t loop_cnt = 0;
  for (auto dom_x : reverse_view(dom->dfn)) {
    auto x = dom_x->bb;
    auto &xi = loop_tree[x];
    for (auto y : xi.in) {
      if (xi.dom_tree_node->dom(loop_tree[y].dom_tree_node)) {
        // std::cerr << $(x) << " <- " << $(y) << '\n';
        xi.is_loop_head = 1;
        find_loop(y, x);
      }
    }
    if (xi.is_loop_head) {
      build_dag_dfn(x, x);
      ++loop_cnt;
    }
  }

  build_dag_dfn(func->entry, nullptr);

  check_dag(nullptr);
  // std::cerr << check_cnt << "  ;  " << reach_cnt << '\n';
  assert(check_cnt - 1 == reach_cnt);
  // std::cerr << func->name << ": " << loop_cnt << '/' << reach_cnt << '\n';
}

std::ostream &operator<<(std::ostream &os, const arg_name_t &arg) {
  os << arg.first->name << ".arg" << arg.second;
  return os;
}
std::ostream &operator<<(std::ostream &os, const mem_name_t &ms) {
  if (std::holds_alternative<MemObject *>(ms)) {
    os << std::get<MemObject *>(ms)->name;
  } else {
    os << std::get<arg_name_t>(ms);
  }
  return os;
}
std::ostream &operator<<(std::ostream &os, const mem_set_t &ms) {
  os << "[ ";
  for (auto x : ms) {
    os << (x ? x->name : "*") << ' ';
  }
  os << ']';
  return os;
}

struct PrintLoopTree : SimpleLoopVisitor {
  PrintLoopTree() {
    dbg("loop tree\n");
    dbg("```mermaid\n");
    dbg("graph TB\n");
    dbg("root(root)\n");
  }
  ~PrintLoopTree() { dbg("```\n"); }
  std::vector<BB *> loop_head{nullptr};
  void begin(BB *bb, bool flag) {
    if (flag)
      loop_head.push_back(bb);
  }
  void end(BB *bb) {
    if (loop_head.back() == bb)
      loop_head.pop_back();
  }
  void visitBB(BB *w) {
    dbg(w->name);
    BB *head = loop_head.back();
    if (head == w) {
      dbg("[", w->name, "]\n");
      head = *std::prev(loop_head.end(), 2);
      dbg((head ? head->name : "root"), "-->", w->name, "\n");
    } else {
      dbg("(", w->name, ")\n");
      dbg((head ? head->name : "root"), "-->", w->name, "\n");
    }
  }
};

struct PointerBase : InstrVisitor {
  struct Info {
    mem_name_t base;
    mem_set_t *maybe = nullptr;
  };
  std::unordered_map<Reg, Info> info;

  NormalFunc *func;
  PointerBase(NormalFunc *_func) : func(_func) {}
  void visit(Instr *x) override {
    Case(ArrayIndex, ai, x) { info[ai->d1].base = info.at(ai->s1).base; }
    else Case(PhiInstr, phi, x) {
      for (auto &[r, bb] : phi->uses) {
        if (info.count(r)) {
          info[phi->d1].base = info.at(r).base;
          (void)bb;
          break;
        }
      }
    }
    else Case(UnaryOpInstr, uop, x) {
      if (uop->op.type == UnaryCompute::ID) {
        if (info.count(uop->s1)) {
          info[uop->d1].base = info.at(uop->s1).base;
        }
      }
    }
    else Case(LoadArg, la, x) {
      info[la->d1].base = std::make_pair(func, la->id);
    }
    else Case(LoadAddr, la, x) {
      info[la->d1].base = la->offset;
    }
  }
};
struct SideEffect {
  struct Info {
    mem_set_t may_read, may_write;
    void operator|=(const Info &w) {
      may_read.insert(w.may_read.begin(), w.may_read.end());
      may_write.insert(w.may_write.begin(), w.may_write.end());
    }
  };
  std::unordered_map<BB *, Info> info;
  std::unordered_map<BB *, Info> loop_info;
  NormalFunc *func;
  std::map<NormalFunc *, std::unique_ptr<SideEffect>> *mp;
  PointerBase ptr_base;

  SideEffect(NormalFunc *_func,
             std::map<NormalFunc *, std::unique_ptr<SideEffect>> *_mp)
      : func(_func), mp(_mp), ptr_base(_func) {}
  std::vector<BB *> head{nullptr};
  void begin(BB *bb, bool) { head.push_back(bb); }
  void end(BB *bb) {
    head.pop_back();
    loop_info[head.back()] |= loop_info[bb];
  }
  void ins(mem_set_t &ls, mem_set_t &rs) { ls.insert(rs.begin(), rs.end()); }
  bool checkWAR(mem_set_t &ws, mem_set_t &rs) {
    if (rs.empty() || ws.empty())
      return 0;
    if (rs.count(nullptr) || ws.count(nullptr))
      return 1;
    for (auto x : ws)
      if (rs.count(x))
        return 1;
    return 0;
  }
  mem_set_t &maybe(Reg r) {
    if (!ptr_base.info.count(r))
      return no_mem;
    return *ptr_base.info.at(r).maybe;
  }
  mem_set_t &may_read(Func *f) {
    Case(NormalFunc, f0, f) {
      return mp->at(f0)->loop_info.at(nullptr).may_read;
    }
    else {
      return any_mem;
    }
  }
  mem_set_t &may_write(Func *f) {
    Case(NormalFunc, f0, f) {
      return mp->at(f0)->loop_info.at(nullptr).may_write;
    }
    else {
      return any_mem;
    }
  }
  void visitBB(BB *bb) {
    auto &w = info[bb];
    bb->for_each([&](Instr *x) {
      Case(LoadInstr, ld, x) {
        ins(w.may_read, *ptr_base.info.at(ld->addr).maybe);
      }
      else Case(StoreInstr, st, x) {
        ins(w.may_write, *ptr_base.info.at(st->addr).maybe);
      }
      else Case(CallInstr, call, x) {
        Case(NormalFunc, f, call->f) {
          if (f == func) {
            w.may_read.insert(nullptr);
            w.may_write.insert(nullptr);
          } else {
            w |= mp->at(f)->loop_info.at(nullptr);
          }
        }
        else {
          for (Reg r : call->args) {
            if (ptr_base.info.count(r)) {
              auto &ls = *ptr_base.info.at(r).maybe;
              ins(w.may_read, ls);
              ins(w.may_write, ls);
            }
          }
        }
      }
    });
    loop_info[bb] = w;
  }
  void visitMayExit(BB *, BB *) {}
  void visitEdge(BB *, BB *) {}
};

struct MergePureCall
    : ForwardLoopVisitor<std::map<std::pair<Func *, std::vector<Reg>>, Reg>> {
  using ForwardLoopVisitor::map_t;
  SideEffect &se;
  MergePureCall(SideEffect &_se) : se(_se) {}
  ~MergePureCall() {
    if (cnt) {
      ::info << "MergePureCall: " << cnt << '\n';
    }
  }
  void update(map_t &m, mem_set_t &mw) {
    remove_if(m, [&](typename map_t::value_type &t) -> bool {
      auto &mr = se.may_read(t.first.first);
      return se.checkWAR(mw, mr);
    });
  }
  size_t cnt = 0;
  void visitBB(BB *bb) {
    // std::cerr << bb->name << " visited" << '\n';
    auto &w = info[bb];
    w.out = w.in;
    if (w.is_loop_head) {
      update(w.out, se.loop_info.at(bb).may_write);
    }
    for (auto it = bb->instrs.begin(); it != bb->instrs.end(); ++it) {
      Instr *x = it->get();
      replace_reg(x);
      Case(StoreInstr, st, x) { update(w.out, se.maybe(st->addr)); }
      else Case(CallInstr, call, x) {
        // std::cerr << bb->name << " >>> " << *call << std::endl;
        if (call->no_store) {
          auto key = std::make_pair(call->f, call->args);
          if (w.out.count(key)) {
            replace_reg(it, call->d1, w.out[key]);
            ++cnt;
            // std::cerr << call->f->name << " merged" << std::endl;
          } else {
            w.out[key] = call->d1;
          }
        } else {
          update(w.out, se.may_write(call->f));
        }
      }
    }
  }
};

struct LoadToReg : ForwardLoopVisitor<std::map<Reg, Reg>> {
  using ForwardLoopVisitor::map_t;
  SideEffect &se;
  LoadToReg(SideEffect &_se) : se(_se) {}
  ~LoadToReg() {
    if (cnt) {
      ::info << "LoadToReg: " << cnt << '\n';
    }
  }
  void update(map_t &m, mem_set_t &mw) {
    remove_if(m, [&](typename map_t::value_type &t) -> bool {
      auto &mr = se.maybe(t.first);
      return se.checkWAR(mw, mr);
    });
  }
  size_t cnt = 0;
  void visitBB(BB *bb) {
    // std::cerr << bb->name << " visited" << '\n';
    auto &w = info[bb];
    w.out = w.in;
    if (w.is_loop_head) {
      update(w.out, se.loop_info.at(bb).may_write);
    }
    for (auto it = bb->instrs.begin(); it != bb->instrs.end(); ++it) {
      Instr *x = it->get();
      replace_reg(x);
      Case(LoadInstr, ld, x) {
        if (w.out.count(ld->addr)) {
          replace_reg(it, ld->d1, w.out[ld->addr]);
          ++cnt;
        } else {
          w.out[ld->addr] = ld->d1;
        }
      }
      else Case(StoreInstr, st, x) {
        update(w.out, se.maybe(st->addr));
        w.out[st->addr] = st->s1;
      }
      else Case(CallInstr, call, x) {
        update(w.out, se.may_write(call->f));
      }
    }
  }
};

struct RemoveUnusedStoreInBB : SimpleLoopVisitor {
  size_t cnt = 0;
  ~RemoveUnusedStoreInBB() {
    if (cnt) {
      ::info << "RemoveUnusedStoreInBB: " << cnt << '\n';
    }
  }
  void visitBB(BB *bb) {
    std::set<Reg> cur;

    for (auto it = bb->instrs.end(); it != bb->instrs.begin();) {
      auto it0 = it;
      --it;
      Instr *x = it->get();
      Case(LoadInstr, ld, x) {
        (void)ld;
        cur.clear();
      }
      else Case(StoreInstr, st, x) {
        if (!cur.insert(st->addr).second) {
          ++cnt;
          bb->instrs.erase(it);
          it = it0;
        }
      }
      else Case(CallInstr, call, x) {
        if (!call->no_store) {
          cur.clear();
        }
      }
    }
  }
};

struct RemoveUnusedStore : BackwardLoopVisitor<mem_set_t> {
  SideEffect &se;
  RemoveUnusedStore(SideEffect &_se) : se(_se) {}

  size_t cnt = 0;
  ~RemoveUnusedStore() {
    if (cnt) {
      ::info << "RemoveUnusedStore: " << cnt << '\n';
    }
  }
  void begin(BB *bb, bool is_loop_head) override {
    auto &w = info[bb];
    if (is_loop_head) {
      update(w.loop_out, se.loop_info.at(bb).may_read);
    }
    BackwardLoopVisitor<mem_set_t>::begin(bb, is_loop_head);
  }

  void update(map_t &m, map_t &mr) {
    for (auto x : mr)
      m.insert(x);
  }
  void visitBB(BB *bb) {
    auto &w = info[bb];

    w.in = w.out;

    for (auto it = bb->instrs.end(); it != bb->instrs.begin();) {
      auto it0 = it;
      --it;
      Instr *x = it->get();
      Case(LoadInstr, ld, x) { update(w.in, se.maybe(ld->addr)); }
      else Case(StoreInstr, st, x) {
        if (!se.checkWAR(se.maybe(st->addr), w.in)) {
          ++cnt;
          bb->instrs.erase(it);
          it = it0;
        }
      }
      else Case(CallInstr, call, x) {
        Case(NormalFunc, f, call->f) { update(w.in, se.may_read(f)); }
        else {
          for (Reg r : call->args) {
            update(w.in, se.maybe(r));
          }
        }
      }
    }
  }
  virtual void visitBackEdge(BB *bb1, BB *bb2) {
    auto &w1 = info[bb1]; // loop head
    auto &w2 = info[bb2]; // node before exit (in the view of DAG for loop bb1)
    bool flag = 0;
    meet_eq(w1.loop_out, w2.get_out(), flag);
  }
};

inline void compute_data_offset(CompileUnit &c) {
  c.for_each([](MemScope &s) {
    s.size = 0;
    s.for_each([&](MemObject *x) {
      x->offset = s.size;
      s.size += x->size;
    });
  });
}

void DAG_IR_ALL::update_alias() {
  ir->for_each([&](NormalFunc *f) {
    auto &info = effect[f]->ptr_base.info;
    f->for_each([&](Instr *x) {
      Case(CallInstr, call, x) {
        Case(NormalFunc, f, call->f) {
          for (auto [r, id] : enumerate(call->args)) {
            if (info.count(r)) {
              alias.emplace(info[r].base, arg_name_t{f, id});
            }
          }
        }
      }
    });
  });
  for (bool flag;;) {
    flag = 0;
    for (auto &[a, b] : alias) {
      auto &as = memobjs[a];
      auto &bs = memobjs[b];
      for (auto x : as) {
        flag |= bs.insert(x).second;
      }
    }
    if (!flag)
      break;
  }
  ir->for_each([&](NormalFunc *f) {
    for (auto &w : effect[f]->ptr_base.info) {
      w.second.maybe = &memobjs[w.second.base];
    }
  });
}
void DAG_IR_ALL::print() {
  for (auto &[k, v] : memobjs) {
    std::cerr << k << " : " << v << '\n';
  }
  ir->for_each([&](NormalFunc *f) {
    std::cerr << f->name << " : \n";
    for (auto &[k, v] : effect.at(f)->ptr_base.info) {
      std::cerr << k << " : " << v.base << " : " << *v.maybe << '\n';
    }
    std::cerr << f->name << " R: " << effect.at(f)->may_read(f) << '\n';
    std::cerr << f->name << " W: " << effect.at(f)->may_write(f) << '\n';
  });
}
void DAG_IR_ALL::remove_unused_memobj() {
  std::set<MemObject *> used;
  ir->for_each([&](NormalFunc *f) {
    f->for_each([&](Instr *x) {
      Case(LoadAddr, la, x) { used.insert(la->offset); }
    });
  });
  ir->for_each([&](MemScope &ms) {
    remove_if_vec(ms.objects, [&](const std::unique_ptr<MemObject> &mem) {
      return !used.count(mem.get());
    });
  });
}

void remove_phi(NormalFunc *);
void code_reorder(NormalFunc *);
void remove_trivial_BB(NormalFunc *);

DAG_IR_ALL::DAG_IR_ALL(CompileUnit *_ir, PassType type) : ir(_ir) {
  remove_unused_memobj();
  remove_unused_BB();
  if (type == REMOVE_UNUSED_BB)
    return;
  if (type == BEFORE_BACKEND) {
    ir->for_each([&](NormalFunc *f) {
      remove_phi(f);
      code_reorder(f);
      remove_trivial_BB(f);
    });
    compute_data_offset(*ir);
    return;
  }
  if (!typed) {
    std::cerr << "type check failed\n";
    return;
  }
  PassDisabled("dag") return;
  ir->for_each([&](MemScope &ms) {
    ms.for_each([&](MemObject *mem) { memobjs[mem] = {mem}; });
  });
  ir->for_each([&](NormalFunc *f) {
    dags.emplace(f, new DAG_IR(f));
    effect.emplace(f, new SideEffect(f, &effect));
    dags[f]->visit(effect[f]->ptr_base);
  });
  update_alias();
  ir->for_each([&](NormalFunc *f) { dags[f]->visit(*effect[f]); });
  // print();
  for (auto &[f, dag] : dags) {
    SideEffect &se = *effect[f];
    {
      LoadToReg w(se);
      dag->visit(w);
    }
    {
      MergePureCall w(se);
      dag->visit(w);
    }
    {
      RemoveUnusedStoreInBB w;
      dag->visit(w);
    }
    if (f == ir->main()) {
      RemoveUnusedStore w(se);
      dag->visit_rev(w);
    }
  }
}

void dag_ir(CompileUnit *ir) { DAG_IR_ALL _(ir, NORMAL); }
void remove_unused_BB(CompileUnit *ir) { DAG_IR_ALL _(ir, REMOVE_UNUSED_BB); }
void before_backend(CompileUnit *ir) { DAG_IR_ALL _(ir, BEFORE_BACKEND); }
