#include "ir/opt/dom.hpp"
#include "ir/opt/opt.hpp"
using namespace IR;

void _dbg1() {}
template <class T1, class... T2> void _dbg1(const T1 &x, const T2 &... xs) {
  if (global_config.log_level > 0)
    return;
  std::cerr << x;
  _dbg1(xs...);
}
#define dbg(...) _dbg1(__VA_ARGS__)

void print_cfg(NormalFunc *f) {
  dbg("CFG:\n");
  dbg("```mermaid\n");
  dbg("graph TB\n");
  f->for_each([&](BB *w) { dbg(w->name, "(", w->name, ")\n"); });
  f->for_each([&](BB *w) {
    for (BB *u : w->getOutNodes())
      dbg(w->name, "-->", u->name, "\n");
  });
  dbg("```\n");
}

struct DAG_IR {
  NormalFunc *func;
  std::unique_ptr<DomTreeContext> dom;

  struct LoopTreeNode {
    DomTreeNode *dom_tree_node = nullptr;
    std::vector<BB *> in, out;
    std::vector<BB *> loop_ch, loop_exit, back_edge, may_exit;
    BB *loop_head = nullptr;
    bool reachable = 0, returnable = 0, is_loop_head = 0, visited = 0,
         dfn_visited = 0;
    std::vector<BB *> dfn;
  };
  std::unordered_map<BB *, LoopTreeNode> loop_tree;
  size_t reach_cnt = 0;
  void rev_traverse(BB *w) {
    auto &wi = loop_tree[w];
    if (!wi.reachable || wi.returnable)
      return;
    wi.returnable = 1;
    for (auto u : wi.in)
      rev_traverse(u);
  }
  void traverse(BB *w) {
    auto &wi = loop_tree[w];
    if (wi.reachable)
      return;
    wi.reachable = 1;
    ++reach_cnt;
    // std::cerr << "reach" << $(w) << std::endl;
    for (auto u : wi.out)
      traverse(u);
  }
  void find_loop(BB *w, BB *head) {
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
  std::string $(BB *w) { return w ? w->name : "(null)"; }
  void _build_dag_dfn(BB *w, BB *head, BB *prev = nullptr) {
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
  void build_dag_dfn(BB *w, BB *head) {
    _build_dag_dfn(w, head);
    auto &hi = loop_tree[head];
    std::reverse(hi.dfn.begin(), hi.dfn.end());
    hi.dfn_visited = 0;
  }
  void print(BB *head) {
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
  size_t check_cnt = 0;
  void check_dag(BB *head) {
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
  template <class T> void visit(T &f) { _visit(nullptr, f); }
  template <class T> void _visit(BB *w, T &f) {
    auto &wi = loop_tree[w];
    f.begin(w, wi.is_loop_head);
    for (auto u : wi.dfn) {
      if (u != w) {
        _visit(u, f);
      } else {
        f.visitBB(w);
      }
      // std::cerr << "node: " << u->name << " : " << w->name << '\n';
      auto &ui = loop_tree[u];
      for (auto v : u == w ? ui.out : ui.loop_exit) {
        auto &vi = loop_tree[v];
        // std::cerr << "edge: " << u->name << "->" << v->name << '\n';
        if (v != w && vi.loop_head == w) {
          f.visitEdge(u, v);
        }
      }
    }
    if (wi.is_loop_head) {
      for (auto u : wi.may_exit)
        f.visitMayExit(w, u);
    }
    f.end(w);
  }
  template <class T> void visit_rev(T &f) { _visit_rev(nullptr, f); }
  template <class T> void _visit_rev(BB *w, T &f) {
    auto &wi = loop_tree[w];
    f.begin(w, wi.is_loop_head);
    if (wi.is_loop_head) {
      for (auto u : wi.may_exit)
        f.visitMayExit(w, u);
    }
    for (auto u : reverse_view(wi.dfn)) {
      auto &ui = loop_tree[u];
      if (u != w && ui.is_loop_head)
        f.visitLoopHead(u);
      for (auto v : u == w ? ui.out : ui.loop_exit) {
        auto &vi = loop_tree[v];
        if (v == w) {
          f.visitBackEdge(w, u);
        }
        if (vi.loop_head == w) {
          f.visitEdge(u, v);
        }
      }
      if (u != w) {
        _visit_rev(u, f);
      } else {
        f.visitBB(w);
      }
    }
    f.end(w);
  }
  DAG_IR(NormalFunc *_func) : func(_func) {
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
};

struct ReplaceReg {
  std::unordered_map<Reg, Reg> reg_map;
  void replace_reg(std::list<std::unique_ptr<Instr>>::iterator it, Reg d1,
                   Reg s1) {
    *it = std::make_unique<UnaryOpInstr>(d1, s1, UnaryCompute::ID);
    reg_map[d1] = s1;
  }
  void replace_reg(Instr *x) {
    x->map_use([&](Reg &r) {
      if (reg_map.count(r))
        r = reg_map[r];
    });
  }
};

template <class T> struct LoopVisitor {
  typedef T map_t;
  struct Info {
    // dataflow info at the beginning of this (loop|BB)
    map_t in;
    // dataflow info at the end of this BB
    map_t out;
    // dataflow info at the end of this loop
    map_t loop_out;
    bool visited = 0, is_loop_head = 0, in_loop = 0;
    bool loop_visited = 0;
    map_t &get_out() { return is_loop_head && !in_loop ? loop_out : out; }
  };
  std::unordered_map<BB *, Info> info;
  virtual void begin(BB *bb, bool is_loop_head) {
    // begin visit (loop|BB) bb (loop head is bb)
    auto &w = info[bb];
    w.is_loop_head = is_loop_head;
    w.in_loop = 1;
  }
  virtual void end(BB *bb) {
    // end visit loop bb (loop head is bb)
    auto &w = info[bb];
    w.in_loop = 0;
  }
  virtual void meet_eq(map_t &a, map_t &b, bool &flag) = 0;
  virtual void visitMayExit(BB *bb1, BB *bb2) {
    auto &w1 = info[bb1]; // loop head
    auto &w2 = info[bb2]; // node before exit (in the view of DAG for loop bb1)
    meet_eq(w1.loop_out, w2.get_out(), w1.loop_visited);
  }
  virtual void visitEdge(BB *bb1, BB *bb2) {
    auto &w1 = info[bb1]; // from
    auto &w2 = info[bb2]; // to
    meet_eq(w2.in, w1.get_out(), w2.visited);
  }
};

template <class T> struct ForwardLoopVisitor : LoopVisitor<T>, ReplaceReg {
  typedef T map_t;
  void meet_eq(map_t &a, map_t &b, bool &flag) {
    if (!flag) {
      flag = 1;
      a = b;
      return;
    }
    remove_if(a, [&](typename map_t::value_type &x) {
      auto &[k, v] = x;
      auto it = b.find(k);
      return !(it != b.end() && (it->second == v));
    });
  }
};

template <class T> struct BackwardLoopVisitor : LoopVisitor<T>, ReplaceReg {
  typedef T map_t;
  void meet_eq(map_t &a, map_t &b, bool &flag) {
    flag = 1;
    for (auto &x : a)
      b.insert(x);
  }
  virtual void visitLoopHead(BB *bb) {
    auto &w = LoopVisitor<T>::info[bb];
    w.is_loop_head = 1;
  }
  virtual void visitBackEdge(BB *bb1, BB *bb2) = 0;
};

struct SimpleLoopVisitor {
  void begin(BB *, bool) {}
  void end(BB *) {}
  void visitMayExit(BB *, BB *) {}
  void visitEdge(BB *, BB *) {}
};

struct CodeReorder : SimpleLoopVisitor {
  std::vector<std::unique_ptr<BB>> bbs;
  void visitBB(BB *bb) { bbs.emplace_back(bb); }
  void apply(NormalFunc *f) {
    for (auto &x : f->bbs)
      x.release();
    f->bbs = std::move(bbs);
  }
};

struct InstrVisitor : SimpleLoopVisitor {
  virtual void visit(Instr *) = 0;
  void visitBB(BB *bb) {
    bb->for_each([&](Instr *x) { visit(x); });
  }
};

typedef std::pair<NormalFunc *, int> arg_name_t;
typedef std::variant<MemObject *, arg_name_t> mem_name_t;
typedef std::set<MemObject *> mem_set_t;
mem_set_t any_mem{nullptr}, no_mem{};

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

struct TypeCheck : InstrVisitor {
  enum Type {
    Int,
    Float,
    Addr,
  };
  typedef std::variant<Type, Reg> node_t;
  friend std::ostream &operator<<(std::ostream &os, node_t x) {
    if (std::holds_alternative<Type>(x)) {
      switch (std::get<Type>(x)) {
      case Int:
        os << "int";
        break;
      case Float:
        os << "float";
        break;
      case Addr:
        os << "addr";
        break;
      }
    } else {
      os << std::get<Reg>(x);
    }
    return os;
  }
  UnionFind<node_t> mp;
  NormalFunc *f;
  TypeCheck(NormalFunc *_f) : f(_f) {}
  Type type(ScalarType x) {
    if (x == ScalarType::Int)
      return Int;
    if (x == ScalarType::Float)
      return Float;
    assert(0);
    return Int;
  }
  bool success() {
    return mp[Int] != mp[Float] && mp[Int] != mp[Addr] && mp[Float] != mp[Addr];
  }
  void visit(Instr *w0) override {
    // ::info << *w0 << '\n';
    auto merge = [&](node_t x, node_t y) {
      // ::info << x << " merge " << y << '\n';
      mp.merge(x, y);
      if (mp[Int] == mp[Float] || mp[Addr] == mp[Float]) {
        ::info << "bad type: " << *w0 << '\n';
        ::debug << '\n' << *f << '\n';
        assert(0);
      }
    };
    Case(LoadAddr, w, w0) { merge(w->d1, Addr); }
    else Case(LoadConst<int32_t>, w, w0) {
      merge(w->d1, Int);
    }
    else Case(LoadConst<float>, w, w0) {
      merge(w->d1, Float);
    }
    else Case(LoadArg, w, w0) {
      (void)w;
    }
    else Case(UnaryOpInstr, w, w0) {
      if (w->op.type == UnaryCompute::ID) {
        merge(w->d1, w->s1);
      } else {
        merge(w->d1, type(w->op.ret_type()));
        merge(w->s1, type(w->op.input_type()));
      }
    }
    else Case(BinaryOpInstr, w, w0) {
      merge(w->d1, type(w->op.ret_type()));
      merge(w->s1, type(w->op.input_type()));
      merge(w->s2, type(w->op.input_type()));
    }
    else Case(ArrayIndex, w, w0) {
      merge(w->d1, Addr);
      merge(w->s1, Addr);
      merge(w->s2, Int);
    }
    else Case(LoadInstr, w, w0) {
      merge(w->addr, Addr);
    }
    else Case(StoreInstr, w, w0) {
      merge(w->addr, Addr);
    }
    else Case(JumpInstr, w, w0) {
      (void)w;
    }
    else Case(BranchInstr, w, w0) {
      merge(w->cond, Int);
    }
    else Case(ReturnInstr, w, w0) {
      (void)w;
    }
    else Case(CallInstr, w, w0) {
      (void)w;
    }
    else Case(PhiInstr, w, w0) {
      for (auto &kv : w->uses) {
        merge(w->d1, kv.first);
      }
    }
    else assert(0);
  }
};

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

struct CondProp : ForwardLoopVisitor<std::map<std::pair<Reg, BB *>, int32_t>> {
  using ForwardLoopVisitor::map_t;
  CondProp() {}
  ~CondProp() {
    if (cnt) {
      ::info << "CondProp: " << cnt << '\n';
    }
  }
  size_t cnt = 0;
  void visitBB(BB *bb) {
    auto &w = info[bb];
    w.out.clear();
    for (auto [k, v] : w.in) {
      if (k.second == nullptr || k.second == bb) {
        w.out[{k.first, nullptr}] = v;
      }
    }
    Case(BranchInstr, br, bb->back()) {
      if (w.out.count({br->cond, nullptr})) {
        auto v = w.out[{br->cond, nullptr}];
        auto target = (v ? br->target1 : br->target0);
        bb->pop();
        bb->push(new JumpInstr(target));
        ++cnt;
      } else {
        w.out[{br->cond, br->target1}] = 1;
        w.out[{br->cond, br->target0}] = 0;
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
    dbg("================\n");
  }
  void begin(BB *bb, bool is_loop_head) override {
    auto &w = info[bb];
    if (is_loop_head) {
      // w.is_loop_head = 1;
      update(w.loop_out, se.loop_info.at(bb).may_read);
      dbg(bb->name, " loop_out: ", w.loop_out, '\n');
      dbg("loop mayread: ", se.loop_info.at(bb).may_read, '\n');
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
    dbg("--------\n");
    dbg("> ", bb->name, " out: ", w.out, '\n');

    for (auto it = bb->instrs.end(); it != bb->instrs.begin();) {
      auto it0 = it;
      --it;
      Instr *x = it->get();
      dbg(*x, '\n');
      Case(LoadInstr, ld, x) { update(w.in, se.maybe(ld->addr)); }
      else Case(StoreInstr, st, x) {
        if (!se.checkWAR(se.maybe(st->addr), w.in)) {
          ++cnt;
          bb->instrs.erase(it);
          it = it0;
          dbg(">>> del\n");
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

    dbg("> ", bb->name, " in: ", w.in, '\n');
  }
  /*void visitMayExit(BB *bb1, BB *bb2) {
    auto &w1 = info[bb1]; // loop head
    auto &w2 = info[bb2]; // node before exit (in the view of DAG for loop bb1)
    meet_eq(w1.loop_out, w2.get_out(), w1.loop_visited);
    dbg(bb1->name, " may exit ", bb2->name, '\n');
    dbg(bb1->name, " loop_out: ", w1.loop_out, '\n');
    dbg(bb2->name, (&w2.get_out() == &w2.loop_out ? " loop_out: " : " out: "),
        w2.get_out(), '\n');
  }
  void visitEdge(BB *bb1, BB *bb2) {
    auto &w1 = info[bb1]; // from
    auto &w2 = info[bb2]; // to
    meet_eq(w2.in, w1.get_out(), w2.visited);
    dbg(bb1->name, " -> ", bb2->name, '\n');
    dbg(bb2->name, " in: ", w2.in, '\n');
    dbg(bb1->name, (&w1.get_out() == &w1.loop_out ? " loop_out: " : " out: "),
        w1.get_out(), '\n');
  }*/
  virtual void visitBackEdge(BB *bb1, BB *bb2) {
    auto &w1 = info[bb1]; // loop head
    auto &w2 = info[bb2]; // node before exit (in the view of DAG for loop bb1)
    bool flag = 0;
    meet_eq(w1.loop_out, w2.get_out(), flag);
    dbg(bb1->name, " <- ", bb2->name, '\n');
    dbg(bb1->name, " loop_out: ", w1.loop_out, '\n');
    dbg(bb2->name, (&w2.get_out() == &w2.loop_out ? " loop_out: " : " out: "),
        w2.get_out(), '\n');
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

enum PassType { NORMAL, REMOVE_UNUSED_BB, BEFORE_BACKEND };

struct DAG_IR_ALL {
  CompileUnit *ir;
  std::map<NormalFunc *, std::unique_ptr<DAG_IR>> dags;
  std::map<NormalFunc *, std::unique_ptr<SideEffect>> effect;
  std::map<mem_name_t, mem_set_t> memobjs;
  std::set<std::pair<mem_name_t, arg_name_t>> alias;
  void update_alias() {
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
  void print() {
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
  void remove_unused_memobj() {
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
  void simplify_branch(NormalFunc *f) {
    DAG_IR dag(f);
    CondProp cp;
    dag.visit(cp);

    size_t cnt = 0;
    auto defs = build_defs(f);
    f->for_each([&](BB *bb) {
      Case(BranchInstr, br, bb->back()) {
        Case(LoadConst<int32_t>, lc, defs.at(br->cond)) {
          auto target = lc->value ? br->target1 : br->target0;
          bb->pop();
          bb->push(new JumpInstr(target));
          ++cnt;
        }
      }
    });
    if (cnt) {
      ::info << "simplify_branch: " << cnt << '\n';
    }
  }
  bool type_check(NormalFunc *f) {
    DAG_IR dag(f);
    TypeCheck w(f);
    dag.visit(w);
    return w.success();
  }
  void code_reorder(NormalFunc *f) {
    DAG_IR dag(f);
    CodeReorder w;
    dag.visit(w);
    w.apply(f);
  }
  void remove_trivial_BB(NormalFunc *f) {
    auto prev = build_prev(f);
    f->for_each([&](BB *bb) {
      if (prev[bb].size() != 1)
        return;
      BB *bb0 = prev[bb][0];
      Case(JumpInstr, _, bb0->back()) { (void)_; }
      else return;
      assert(bb0 != bb);
      bb->for_each_until([&](Instr *x) -> bool {
        Case(PhiInstr, _, x) {
          (void)_;
          return 1;
        }
        Case(ControlInstr, _, x) {
          (void)_;
          return 1;
        }
        bb0->push1(x);
        bb->move();
        return 0;
      });
    });
    UnionFind<BB *> mp1, mp2;
    f->for_each([&](BB *bb) {
      mp1.add(bb);
      mp2.add(bb);
    });
    auto cplx = [&](BB *bb) {
      return !(prev[bb].size() == 1 && bb->instrs.size() == 1);
    };
    std::unordered_set<BB *> n1, n2;

    f->for_each([&](BB *bb) {
      Case(BranchInstr, br, bb->back()) {
        bool t1 = !cplx(br->target1);
        bool t0 = !cplx(br->target0);
        if (t1) {
          if (t0) {
            n1.insert(br->target1);
          } else {
            n2.insert(br->target0);
          }
        } else if (t0) {
          n2.insert(br->target1);
        }
      }
    });
    f->for_each([&](BB *bb) {
      if (!cplx(bb) && !n1.count(bb)) {
        Case(JumpInstr, jmp, bb->instrs.back().get()) {
          if (!n2.count(jmp->target)) {
            mp1.merge(bb, jmp->target);
            mp2.merge(bb, prev[bb][0]);
          }
        }
      }
    });
    f->for_each([&](BB *bb) {
      bb->for_each([&](Instr *x) {
        Case(PhiInstr, p, x) {
          p->map_BB([&](BB *&w) { w = mp2[w]; });
        }
        else {
          x->map_BB([&](BB *&w) { w = mp1[w]; });
        }
      });
    });
    remove_if_vec(f->bbs, [&](const std::unique_ptr<BB> &bb) {
      return mp1[bb.get()] != bb.get();
    });
    f->for_each([&](BB *bb) {
      Case(BranchInstr, br, bb->back()) { assert(br->target1 != br->target0); }
    });
  }
  void remove_phi(NormalFunc *f) {
    std::unordered_map<std::pair<BB *, BB *>, std::vector<std::pair<Reg, Reg>>>
        movs;
    f->for_each([&](BB *bb) {
      bb->for_each([&](Instr *x) {
        Case(PhiInstr, phi, x) {
          for (auto &[r, bb0] : phi->uses) {
            if (r != phi->d1)
              movs[{bb0, bb}].emplace_back(r, phi->d1);
          }
          bb->move();
        }
      });
    });
    for (auto &[edge, P] : movs) {
      auto [bb0, bb] = edge;
      BB *bb1 = f->new_BB();
      bb0->back()->map_BB(partial_map(bb, bb1));
      bb1->push(new JumpInstr(bb));
      auto emit_copy = [&](Reg a, Reg b) {
        bb1->push1(new UnaryOpInstr(b, a, UnaryCompute::ID));
      };
      Reg n = f->new_Reg();
      std::vector<std::pair<Reg, Reg>> res;
      std::map<Reg, std::optional<Reg>> loc, pred;
      std::vector<Reg> to_do, ready;
      for (auto [a, b] : P) {
        loc[a] = a;
        pred[b] = a;
        to_do.push_back(b);
      }
      for (auto [a, b] : P) {
        if (!loc[b])
          ready.push_back(b);
      }
      while (!to_do.empty()) {
        while (!ready.empty()) {
          Reg b = ready.back();
          ready.pop_back();
          Reg a = pred[b].value();
          Reg c = loc[a].value();
          emit_copy(c, b);
          loc[a] = b;
          if (a == c && pred[a])
            ready.push_back(a);
        }
        Reg b = to_do.back();
        to_do.pop_back();
        if (b == loc[pred[b].value()].value()) {
          emit_copy(b, n);
          loc[b] = n;
          ready.push_back(b);
        }
      }
    }
  }
  void remove_unused_BB(NormalFunc *f) {
    DAG_IR dag(f);
    auto used = [&](BB *bb) {
      auto &w = dag.loop_tree[bb];
      return w.returnable;
    };
    f->for_each([&](BB *bb) {
      bb->for_each([&](Instr *x) {
        Case(PhiInstr, phi, x) {
          remove_if_vec(phi->uses, [&](const std::pair<Reg, BB *> &w) {
            return !used(w.second);
          });
        }
      });
      Case(BranchInstr, br, bb->back()) {
        std::optional<BB *> target;
        if (!used(br->target1)) {
          target = br->target0;
        } else if (!used(br->target0)) {
          target = br->target1;
        }
        if (target) {
          bb->pop();
          bb->push(new JumpInstr(*target));
        }
      }
    });
    remove_if_vec(
        f->bbs, [&](const std::unique_ptr<BB> &bb) { return !used(bb.get()); });
    if (f->bbs.empty()) {
      Reg r = f->new_Reg();
      BB *bb = f->entry = f->new_BB();
      bb->push(new LoadConst<int32_t>(r, 0));
      bb->push(new ReturnInstr(r, 1));
    }
  }
  bool typed = 1;
  void remove_unused_BB() {
    PassDisabled("rub") return;
    ir->for_each([&](NormalFunc *f) {
      PassEnabled("sb") simplify_branch(f);
      remove_unused_BB(f);
      PassEnabled("rtb") {
        code_reorder(f);
        remove_trivial_BB(f);
      }
      typed &= type_check(f);
    });
  }
  DAG_IR_ALL(CompileUnit *_ir, PassType type) : ir(_ir) {
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
        print_cfg(f);
        {
          PrintLoopTree w;
          dag->visit(w);
        }
        dbg("```\n", *f, "```\n");
        dbg("```\n");
        RemoveUnusedStore w(se);
        dag->visit_rev(w);
        dbg("```\n");
      }
    }
  }
};

void dag_ir(CompileUnit *ir) { DAG_IR_ALL _(ir, NORMAL); }
void remove_unused_BB(CompileUnit *ir) { DAG_IR_ALL _(ir, REMOVE_UNUSED_BB); }
void before_backend(CompileUnit *ir) { DAG_IR_ALL _(ir, BEFORE_BACKEND); }
