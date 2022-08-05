#include "ir/opt/dag_ir.hpp"

typedef std::pair<MemObject *, MemSize> const_mem_addr_t;

struct PointerBase : InstrVisitor, Defs {
  struct Info {
    mem_name_t base;
    mem_set_t *maybe = nullptr;
    std::optional<const_mem_addr_t> mustbe;
  };
  std::unordered_map<Reg, Info> info;

  PointerBase(NormalFunc *_func) : Defs(_func) {}
  void visit(Instr *x) override {
    Case(ArrayIndex, ai, x) {
      auto &w = info[ai->d1];
      auto &w0 = info.at(ai->s1);
      w.base = w0.base;
      if (w0.mustbe) {
        auto c = get_const(ai->s2);
        if (c) {
          w.mustbe = w0.mustbe;
          w.mustbe->second += *c * ai->size;
        }
      }
    }
    else Case(PhiInstr, phi, x) {
      for (auto &[r, bb] : phi->uses) {
        if (info.count(r)) {
          auto &i1 = info[phi->d1];
          auto &i2 = info.at(r);
          i1.base = i2.base;
          i1.mustbe = i2.mustbe;
          (void)bb;
          break;
        }
      }
    }
    else Case(UnaryOpInstr, uop, x) {
      if (uop->op.type == UnaryCompute::ID) {
        if (info.count(uop->s1)) {
          auto &d1 = info[uop->d1];
          auto &s1 = info.at(uop->s1);
          d1.base = s1.base;
          d1.mustbe = s1.mustbe;
        }
      }
    }
    else Case(LoadArg, la, x) {
      auto &w = info[la->d1];
      w.base = std::make_pair(f, la->id);
    }
    else Case(LoadAddr, la, x) {
      auto &w = info[la->d1];
      w.base = la->offset;
      w.mustbe = std::make_pair(la->offset, 0);
    }
  }
};

struct SideEffect : SimpleLoopVisitor {
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
  bool checkWAR(mem_set_t &ws, MemObject *r) {
    return ws.count(nullptr) || ws.count(r);
  }
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
  std::optional<std::pair<MemObject *, MemSize>> mustbe(Reg r) {
    if (!ptr_base.info.count(r))
      return std::nullopt;
    return *ptr_base.info.at(r).mustbe;
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

struct GlobalInitProp : ForwardLoopVisitor<std::map<MemObject *, bool>> {
  SideEffect &se;
  GlobalInitProp(SideEffect &_se, BB *bb, MemScope &scope) : se(_se) {
    scope.for_each([&](MemObject *mem) {
      if (mem->scalar_type == ScalarType::Int) {
        info[bb].in[mem];
      }
    });
  }
  void update(map_t &m, mem_set_t &mw) {
    remove_if(m, [&](typename map_t::value_type &t) -> bool {
      return se.checkWAR(mw, t.first);
    });
  }
  template <class T>
  void lc(std::optional<const_mem_addr_t> v, Reg d1,
          decltype(BB::instrs)::iterator it, T _ = 0) {
    (void)_;
    dbg(*it->get(), " mustbe ", v->first->name, " + ", v->second, " : ",
        v->first->at<T>(v->second), '\n');
    *it = std::make_unique<LoadConst<T>>(d1, v->first->at<T>(v->second));
  };
  void visitBB(BB *bb) {
    // std::cerr << bb->name << " visited" << '\n';
    auto &w = info[bb];
    // dbgs(bb, w.in);
    w.out = w.in;
    if (w.is_loop_head) {
      update(w.out, se.loop_info.at(bb).may_write);
    }
    // dbgs(bb, w.out);
    for (auto it = bb->instrs.begin(); it != bb->instrs.end(); ++it) {
      Instr *x = it->get();
      Case(LoadInstr, ld, x) {
        auto v = se.mustbe(ld->addr);
        if (v && w.out.count(v->first)) {
          if (v->first->scalar_type == ScalarType::Int) {
            lc<int32_t>(v, ld->d1, it);
          } else {
            lc<float>(v, ld->d1, it);
          }
        }
      }
      else Case(StoreInstr, st, x) {
        update(w.out, se.maybe(st->addr));
      }
      else Case(CallInstr, call, x) {
        Case(NormalFunc, f, call->f) { update(w.out, se.may_write(f)); }
        else {
          for (Reg r : call->args) {
            update(w.out, se.maybe(r));
          }
        }
      }
    }
    // dbgs(bb, w.out);
    // dbg(bb->name, " visited\n");
  }
};

struct GlobalInitToGlobalConst {
  PointerBase &pb;
  CompileUnit *ir;
  struct Info {
    bool locked = 0;
    std::unordered_map<size_t, int32_t> kv;
  };
  std::unordered_map<MemObject *, Info> init;
  GlobalInitToGlobalConst(PointerBase &_pb, CompileUnit *_ir)
      : pb(_pb), ir(_ir) {
    assert(pb.f == ir->main());
    ir->scope.for_each([&](MemObject *mem) {
      if (mem->scalar_type == ScalarType::Int && mem->size <= 4 * 64) {
        init[mem];
      }
    });
  }
  void apply() {
    pb.f->entry->for_each_until([&](Instr *x) {
      Case(LoadInstr, ld, x) {
        auto &w = pb.info.at(ld->addr);
        auto mem = std::get<MemObject *>(w.base);
        if (!init.count(mem))
          return 0;
        auto &mp = init[mem];
        if (mp.locked)
          return 0;
        if (w.mustbe) {
          assert(w.mustbe->first == mem);
          auto offset = w.mustbe->second;
          int32_t v = (mp.kv.count(offset) ? mp.kv.at(offset)
                                           : mem->at<int32_t>(offset));
          pb.f->entry->replace(new LoadConst(ld->d1, v));
        } else {
          mp.locked = 1;
        }
      }
      else Case(StoreInstr, st, x) {
        auto &w = pb.info.at(st->addr);
        auto mem = std::get<MemObject *>(w.base);
        if (!init.count(mem))
          return 0;
        auto &mp = init[mem];
        if (mp.locked)
          return 0;
        if (w.mustbe) {
          assert(w.mustbe->first == mem);
          auto offset = w.mustbe->second;
          if (auto v = pb.get_const(st->s1)) {
            mp.kv[offset] = *v;
            pb.f->entry->del();
          } else {
            mp.locked = 1;
          }
        } else {
          mp.locked = 1;
        }
      }
      else Case(CallInstr, call, x) {
        (void)call;
        return 1;
      }
      return 0;
    });
    for (auto &[mem, mp] : init) {
      if (!mp.kv.size())
        continue;
      dbg("GlobalInitToGlobalConst: ", mem->name, '\n');
      if (!mem->initial_value) {
        mem->initial_value = new int32_t[mem->size / 4]();
      }
      for (auto &[k, v] : mp.kv) {
        mem->set<int32_t>(k, v);
      }
    }
  }
};

struct LocalInitToGlobalConst : InstrVisitor {
  PointerBase &pb;
  std::unordered_map<MemObject *, std::unordered_map<size_t, int32_t>> init;
  LocalInitToGlobalConst(PointerBase &_pb) : pb(_pb) {
    pb.f->scope.for_each([&](MemObject *mem) {
      if (mem->scalar_type == ScalarType::Int) {
        init[mem];
      }
    });
  }
  void apply(MemScope &global) {
    std::unordered_map<MemObject *, MemObject *> mp;
    for (auto &[mem, kv] : init) {
      dbg(mem->name, " init ", kv.size(), '\n');
      if (kv.size() * 4 * 16 >= mem->size || kv.size() <= 64) {
        auto w = global.new_MemObject(mem->name + "::to_global");
        w->global = 1;
        w->scalar_type = mem->scalar_type;
        w->dims = mem->dims;
        int32_t *data = new int32_t[mem->size / 4]();
        w->init(data, mem->size);
        for (auto &[k, v] : kv) {
          w->set<int32_t>(k, v);
        }
        mp[mem] = w;
        dbg("LocalInitToGlobalConst: ", mem->name, '\n');
      }
    }
    auto f = partial_map(mp);
    pb.f->for_each([&](BB *bb) {
      bb->for_each([&](Instr *x) {
        Case(LoadAddr, la, x) { f(la->offset); }
        else Case(StoreInstr, st, x) {
          auto &w = pb.info.at(st->addr);
          if (auto base = std::get_if<MemObject *>(&w.base)) {
            if (mp.count(*base)) {
              bb->del();
            }
          }
        }
      });
    });
  }
  void visit(Instr *x) override {
    Case(StoreInstr, st, x) {
      auto &w = pb.info.at(st->addr);
      if (auto base = std::get_if<MemObject *>(&w.base)) {
        // dbg(*st, " base: ", (*base)->name, '\n');
        if (!init.count(*base))
          return;
        if (!w.mustbe) {
          // dbg(*st, " => ", (*base)->name, " + ?\n");
          init.erase(*base);
          return;
        }
        MemSize pos = w.mustbe->second;
        assert(w.mustbe->first == *base);
        if (auto val = pb.get_const(st->s1)) {
          auto &mp = init.at(*base);
          if (mp.count(pos)) {
            auto &v = mp[pos];
            if (v != *val) {
              // dbg(*st, " => ", (*base)->name, " + ", pos, " : ", v, '|',
              // *val,
              // "\n");
              init.erase(*base);
            }
          } else {
            // dbg(*st, " => ", (*base)->name, " + ", pos, " : ", *val, "\n");
            mp[pos] = *val;
          }
        } else {
          // dbg(*st, " => ", (*base)->name, " + ", pos, " : ?\n");
          init.erase(*base);
        }
      }
    }
    else Case(CallInstr, call, x) {
      for (Reg r : call->args) {
        if (!pb.info.count(r))
          continue;
        if (auto base = std::get_if<MemObject *>(&pb.info[r].base)) {
          init.erase(*base);
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
namespace IR {
void compute_data_offset(CompileUnit &c);
}
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
      {
        RemoveUnusedStore w(se);
        dag->visit_rev(w);
      }
      {
        GlobalInitProp w(se, f->entry, ir->scope);
        dag->visit(w);
      }
    }
  }
}
void local_init_to_global(CompileUnit *ir, NormalFunc *f) {
  DAG_IR dag(f);
  PointerBase pb(f);
  dag.visit(pb);

  {
    LocalInitToGlobalConst w(pb);
    dag.visit(w);
    w.apply(ir->scope);
  }
  if (f == ir->main()) {
    GlobalInitToGlobalConst w(pb, ir);
    w.apply();
  }
}

void dag_ir(CompileUnit *ir) {
  static size_t round;
  dbg("DAG IR Round ", ++round, "\n");
  ir->for_each([&](NormalFunc *f) { local_init_to_global(ir, f); });
  DAG_IR_ALL _(ir, NORMAL);
  ir->for_each([&](NormalFunc *f) { loop_ops(f); });
  ir->for_each([&](NormalFunc *f) { local_init_to_global(ir, f); });
}
void remove_unused_BB(CompileUnit *ir) { DAG_IR_ALL _(ir, REMOVE_UNUSED_BB); }
void before_backend(CompileUnit *ir) { DAG_IR_ALL _(ir, BEFORE_BACKEND); }
