#include "ir/opt/dom.hpp"
#include "ir/opt/opt.hpp"
using namespace IR;
std::map<Reg, RegWriteInstr *> build_defs(NormalFunc *func) {
  std::map<Reg, RegWriteInstr *> f;
  func->for_each([&](Instr *x) {
    Case(RegWriteInstr, w, x) { f[w->d1] = w; }
  });
  return f;
}

std::map<BB *, std::vector<BB *>> build_prev(NormalFunc *func) {
  std::map<BB *, std::vector<BB *>> f;
  func->for_each([&](BB *bb) {
    for (BB *w : bb->getOutNodes())
      f[w].push_back(bb);
  });
  return f;
}

struct CallGraph {
  struct Info {
    bool visited = 0, no_store = 1, no_load = 1, ret_used = 0, used = 0;
    std::vector<CallInstr *> calls;
    std::vector<std::pair<NormalFunc *, CallInstr *>> called;
    std::map<Reg, RegWriteInstr *> defs;
  };
  std::map<NormalFunc *, Info> info;
  std::pair<bool, bool> isPure(NormalFunc *f) {
    auto &fi = info[f];
    if (fi.visited)
      return {fi.no_store, fi.no_load};
    fi.visited = 1;
    for (auto call : fi.calls) {
      Case(NormalFunc, f0, call->f) {
        if (f0 != f) {
          auto [p1, p2] = isPure(f0);
          fi.no_store &= call->no_store = p1;
          fi.no_load &= call->no_load = p2;
        }
      }
      else Case(LibFunc, f0, call->f) {
        if (!f0->pure) {
          fi.no_store = 0;
          fi.no_load = 0;
        }
      }
      else assert(0);
    }
    for (auto call : fi.calls) {
      Case(NormalFunc, f0, call->f) {
        if (f0 == f) {
          call->no_store = fi.no_store;
          call->no_load = fi.no_load;
        }
      }
      else Case(LibFunc, f0, call->f) {
        call->no_load = call->no_store = f0->pure;
      }
      else assert(0);
    }
    // std::cerr << f->name << "  pure?  " << fi.no_store << fi.no_load << '\n';
    return {fi.no_store, fi.no_load};
  }
  CompileUnit *ir;
  CallGraph(CompileUnit *_ir) : ir(_ir) {
    ir->for_each([&](NormalFunc *f) {
      auto &fi = info[f];
      f->for_each([&](Instr *x) {
        Case(LoadInstr, ld, x) {
          // std::cerr << f->name << "  load:  " << *ld << '\n';
          fi.no_load = 0;
          (void)ld;
        }
        else Case(StoreInstr, st, x) {
          fi.no_store = 0;
          (void)st;
        }
        else Case(CallInstr, call, x) {
          fi.calls.push_back(call);
        }
      });
    });
    ir->for_each([&](NormalFunc *f) {
      auto &fi = info[f];
      fi.defs = build_defs(f);
      for (auto call : fi.calls) {
        Case(NormalFunc, f0, call->f) { info[f0].called.emplace_back(f, call); }
      }
    });
  }
  void build_pure() {
    ir->for_each([&](NormalFunc *f) { isPure(f); });
  }
  void const_prop() {
    size_t cnt = 0;
    ir->for_each([&](NormalFunc *f) {
      auto &fi = info[f];
      if (fi.called.size() == 1) {
        auto [f0, call] = fi.called[0];
        auto &defs = info[f0].defs;
        std::vector<std::pair<int32_t, int>> args(call->args.size(), {0, 0});
        std::map<Reg, int> same_arg;
        size_t used_arg = 0;
        bool flag = 0;
        for (auto [r, id] : enumerate(call->args)) {
          auto rw = defs.at(r.first);
          Case(LoadConst<int32_t>, lc, rw) {
            flag = 1;
            args[id] = {lc->value, 1};
          }
          else if (same_arg.count(r.first)) {
            flag = 1;
            args[id] = {same_arg[r.first], 2};
          }
          else {
            same_arg[r.first] = id;
            args[id] = {id, 0};
            used_arg = id + 1;
          }
        }
        if (!flag)
          return;
        call->args.resize(used_arg);
        f->for_each([&](BB *bb) {
          bb->for_each([&](Instr *x) {
            Case(LoadArg<ScalarType::Int>, la, x) {
              auto [v, type] = args.at(la->id);
              if (type == 2) {
                la->id = v;
                ++cnt;
              } else if (type == 1) {
                Reg d1 = la->d1;
                auto lc = new LoadConst<int32_t>(d1, v);
                bb->replace(lc);
                info[f].defs.at(d1) = lc;
                ++cnt;
              }
            }
            else Case(LoadArg<ScalarType::Float>, la, x) {
              auto [v, type] = args.at(la->id);
              if (type == 2) {
                la->id = v;
                ++cnt;
              } else if (type == 1) {
                Reg d1 = la->d1;
                auto lc = new LoadConst<float>(d1, v);
                bb->replace(lc);
                info[f].defs.at(d1) = lc;
                ++cnt;
              }
            }
          });
        });
      }
    });
    if (cnt) {
      ::info << "CallGraph.constProp: " << cnt << '\n';
    }
  }
  void remove_unused_ret() {
    size_t cnt = 0;
    info[ir->main()].ret_used = 1;
    for (auto f : reverse_view(ir->_funcs)) {
      auto use = build_use_count(f);
      auto &fi = info[f];
      for (auto call : fi.calls) {
        Case(NormalFunc, f0, call->f) {
          if (use[call->d1])
            info[f0].ret_used = 1;
        }
      }
      if (!fi.ret_used) {
        bool flag = 0;
        f->for_each([&](BB *bb) {
          Case(ReturnInstr<ScalarType::Int>, ret, bb->back()) {
            Case(LoadConst<int32_t>, _, fi.defs.at(ret->s1)) {
              (void)_;
              return;
            }
            Reg r = ret->s1 = f->new_Reg();
            auto lc = new LoadConst(r, 0);
            bb->push1(lc);
            fi.defs[r] = lc;
            flag = 1;
          }
          else Case(ReturnInstr<ScalarType::Float>, ret, bb->back()) {
            Case(LoadConst<float>, _, fi.defs.at(ret->s1)) {
              (void)_;
              return;
            }
            Reg r = ret->s1 = f->new_Reg();
            auto lc = new LoadConst(r, 0.0f);
            bb->push1(lc);
            fi.defs[r] = lc;
            flag = 1;
          }
        });
        if (flag) {
          remove_unused_def_func(f);
          ++cnt;
        }
      }
    }
    if (cnt) {
      ::info << "CallGraph.remove_unused_ret: " << cnt << '\n';
    }
  }
  void tail_rec_to_loop(NormalFunc *f) {
    size_t cnt = 0;
    BB *bb = nullptr;
    f->for_each([&](BB *bb0) {
      Case(ReturnInstr<ScalarType::Int>, _, bb0->back()) {
        (void)_;
        assert(!bb);
        bb = bb0;
      }
      Case(ReturnInstr<ScalarType::Float>, _, bb0->back()) {
        (void)_;
        assert(!bb);
        bb = bb0;
      }
    });
    assert(bb);
    if (bb->instrs.size() != 2)
      return;
    Case(PhiInstr, phi, bb->back1()) {
      auto prev = build_prev(f);
      if (phi->uses.size() != prev[bb].size())
        return;
      std::set<BB *> tail_rec;
      for (auto &[r, bb0] : phi->uses) {
        if (bb0->instrs.size() < 2)
          continue;
        Case(JumpInstr, jmp, bb0->back()) {
          assert(jmp->target == bb);
          Case(CallInstr, call, bb0->back1()) {
            if (call->f == f && call->d1 == r) {
              tail_rec.insert(bb0);
            }
          }
        }
      }
      if (tail_rec.size()) {
        ++cnt;
        std::unordered_map<Reg, Reg> mp;
        std::map<int, std::tuple<Reg, Reg, ScalarType>> args;
        f->for_each([&args, &f, &mp](Instr *x) {
          Case(LoadArg<ScalarType::Int>, la, x) {
            if (!args.count(la->id)) {
              Reg r1 = f->new_Reg();
              Reg r2 = f->new_Reg();
              args[la->id] = {r1, r2, ScalarType::Int};
            }
            mp[la->d1] = std::get<1>(args[la->id]);
          }
          Case(LoadArg<ScalarType::Float>, la, x) {
            if (!args.count(la->id)) {
              Reg r1 = f->new_Reg();
              Reg r2 = f->new_Reg();
              args[la->id] = {r1, r2, ScalarType::Float};
            }
            mp[la->d1] = std::get<1>(args[la->id]);
          }
        });
        auto new_entry = f->new_BB();
        auto old_entry = f->entry;
        f->entry = new_entry;
        for (auto &[id, rs] : args) {
          if (std::get<2>(rs) == ScalarType::Int)
            new_entry->push(new LoadArg<ScalarType::Int>(std::get<0>(rs), id));
          if (std::get<2>(rs) == ScalarType::Float)
            new_entry->push(
                new LoadArg<ScalarType::Float>(std::get<0>(rs), id));
        }
        new_entry->push(new JumpInstr(old_entry));
        for (auto &[id, rs] : args) {
          auto phi = new PhiInstr(std::get<1>(rs));
          old_entry->push_front(phi);
          phi->add_use(std::get<0>(rs), new_entry);
          for (BB *bb0 : tail_rec) {
            Case(CallInstr, call, bb0->back1()) {
              phi->add_use(call->args.at(id).first, bb0);
            }
            else assert(0);
          }
        }
        for (BB *bb0 : tail_rec) {
          bb0->pop();
          bb0->pop();
          bb0->push(new JumpInstr(old_entry));
        }
        f->for_each([&](Instr *x) { x->map_use(partial_map(mp)); });
        remove_if_vec(phi->uses, [&](const std::pair<Reg, BB *> &w) {
          return tail_rec.count(w.second);
        });
        map_use(f, mp);
      }
    }
    if (cnt) {
      ::info << "CallGraph.tail_rec_to_loop: " << cnt << '\n';
    }
  }
  void tail_rec_to_loop() {
    ir->for_each([&](NormalFunc *f) { tail_rec_to_loop(f); });
  }
  void remove_unused_func() {
    info[ir->main()].used = 1;
    for (NormalFunc *f : reverse_view(ir->_funcs)) {
      auto &fi = info[f];
      if (fi.used) {
        for (auto call : fi.calls) {
          Case(NormalFunc, f0, call->f) { info[f0].used = 1; }
        }
      }
    }
    auto unused = [&](NormalFunc *f) { return !info[f].used; };
    remove_if_vec(ir->_funcs, unused);
    remove_if(ir->funcs, [&](const decltype(ir->funcs)::value_type &x) {
      return unused(x.second.get());
    });
  }
  void cache_pure_func(NormalFunc *f) {
    auto &fi = info[f];
    if (!(fi.no_load && fi.no_store))
      return;
    bool is_rec = 0, is_single_arg = 1;
    for (auto call : fi.calls) {
      Case(NormalFunc, f0, call->f) {
        if (f0 == f) {
          is_rec = 1;
          if (call->args.size() != 1)
            is_single_arg = 0;
        }
      }
    }
    if (!is_rec || !is_single_arg)
      return;
    ScalarType arg_t = ScalarType::Void;
    ScalarType ret_t = ScalarType::Void;
    BB *ret_bb = nullptr;
    Reg ret_reg;
    f->for_each([&](BB *bb) {
      bb->for_each([&](Instr *x) {
        Case(LoadArg<ScalarType::Int>, la, x) {
          arg_t = ScalarType::Int;
          (void)la;
        }
        else Case(LoadArg<ScalarType::Float>, la, x) {
          arg_t = ScalarType::Float;
          (void)la;
        }
        else Case(ReturnInstr<ScalarType::Int>, ret, x) {
          ret_t = ScalarType::Int;
          if (ret_bb)
            return;
          ret_bb = bb;
          ret_reg = ret->s1;
        }
        else Case(ReturnInstr<ScalarType::Float>, ret, x) {
          ret_t = ScalarType::Float;
          if (ret_bb)
            return;
          ret_bb = bb;
          ret_reg = ret->s1;
        }
      });
    });
    if (arg_t == ScalarType::Void)
      return;
    if (ret_t == ScalarType::Void)
      return;
    if (!ret_bb)
      return;
    const int N = 10100;
    MemObject *mem_key = ir->scope.new_MemObject(f->name + "::key");
    mem_key->size = 4 * N;
    mem_key->dims = {N};
    mem_key->global = 1;
    mem_key->scalar_type = ScalarType::Int;
    MemObject *mem_val = ir->scope.new_MemObject(f->name + "::val");
    mem_val->size = 4 * N;
    mem_val->dims = {N};
    mem_val->global = 1;
    mem_val->scalar_type = ret_t;
    BB *bb1 = f->new_BB();
    BB *bb2 = f->new_BB();
    BB *bb3 = f->new_BB();
    BB *bb4 = f->new_BB();
    CodeGen cg(f);
    CodeGen::RegRef argv;
    auto f_mov_to_i = ir->lib_funcs.at("__f_mov_to_i").get();

    if (arg_t == ScalarType::Int) {
      argv = cg.la_int(0);
    } else {
      argv = cg.la_float(0);
      argv = cg.call(f_mov_to_i, ScalarType::Int, {{argv, ScalarType::Float}});
    }
    cg.branch(argv != cg.lc(0), bb2, f->entry);
    bb1->push(std::move(cg.instrs));
    auto P = cg.lc(10007);
    auto hash = (argv % P + P) % P;
    auto key = cg.ld(cg.ai(cg.la(mem_key), hash, 4));
    cg.branch(argv == key, bb3, f->entry);
    bb2->push(std::move(cg.instrs));

    auto v1 = cg.ld(cg.ai(cg.la(mem_val), hash, 4));
    cg.jump(bb4);
    bb3->push(std::move(cg.instrs));

    auto v2 = cg.reg(ret_reg);
    cg.st(cg.ai(cg.la(mem_key), hash, 4), argv);
    cg.st(cg.ai(cg.la(mem_val), hash, 4), v2);
    cg.jump(bb4);
    ret_bb->pop();
    ret_bb->push(std::move(cg.instrs));

    Reg v3 = f->new_Reg();
    auto phi = new PhiInstr(v3);
    phi->add_use(v1.r, bb3);
    phi->add_use(v2.r, ret_bb);
    bb4->push(phi);
    if (ret_t == ScalarType::Int) {
      bb4->push(new ReturnInstr<ScalarType::Int>(v3, 0));
    } else {
      bb4->push(new ReturnInstr<ScalarType::Float>(v3, 0));
    }
    f->entry = bb1;
    // dbg(*f);
  }
  void cache_pure_func() {
    ir->for_each([&](NormalFunc *f) { cache_pure_func(f); });
  }
};

void call_graph(CompileUnit *ir) {
  PassDisabled("cg") return;
  CallGraph cg(ir);
  PassEnabled("cgcp") cg.const_prop();
  PassEnabled("pure") cg.build_pure();
  PassEnabled("rur") cg.remove_unused_ret();
  PassEnabled("trtl") cg.tail_rec_to_loop();
  checkIR(ir);
}
void remove_unused_func(CompileUnit *ir) {
  PassDisabled("ruf") return;
  CallGraph cg(ir);
  cg.remove_unused_func();
  checkIR(ir);
}
void cache_pure_func(CompileUnit *ir) {
  PassDisabled("cpf") return;
  {
    CallGraph cg(ir);
    cg.build_pure();
    cg.cache_pure_func();
  }
  {
    CallGraph cg(ir);
    cg.build_pure();
  }
}
