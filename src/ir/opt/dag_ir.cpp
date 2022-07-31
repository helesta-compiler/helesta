#include "ir/opt/dom.hpp"
#include "ir/opt/opt.hpp"
using namespace IR;

template <class T> struct reverse_view {
  T &x;
  reverse_view(T &_x) : x(_x) {}
  auto begin() { return x.rbegin(); }
  auto end() { return x.rend(); }
};

struct DAG_IR {
  NormalFunc *func;
  std::unique_ptr<DomTreeContext> dom;

  struct LoopTreeNode {
    DomTreeNode *dom_tree_node = nullptr;
    std::vector<BB *> in, out;
    std::vector<BB *> loop_ch, loop_exit, back_edge;
    BB *loop_head = nullptr;
    bool reachable = 0, is_loop_head = 0, visited = 0, dfn_visited = 0;
    std::vector<BB *> dfn;
  };
  std::unordered_map<BB *, LoopTreeNode> loop_tree;
  size_t reach_cnt = 0;
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
  void _build_dag_dfn(BB *w, BB *head) {
    auto &wi = loop_tree[w];
    if (wi.dfn_visited)
      return;
    // std::cerr << "build_dag_dfn : " << $(w) << " : " << $(head) << '\n';
    if (w != head && wi.loop_head != head) {
      loop_tree[head].loop_exit.push_back(w);
      return;
    }
    wi.dfn_visited = 1;
    if (w != head && !wi.is_loop_head) {
      wi.loop_exit = wi.out;
      wi.dfn.push_back(w);
    }
    for (auto u : w == head ? wi.out : wi.loop_exit)
      _build_dag_dfn(u, head);
    loop_tree[head].dfn.push_back(w);
  }
  void build_dag_dfn(BB *w, BB *head) {
    _build_dag_dfn(w, head);
    auto &hi = loop_tree[head];
    std::reverse(hi.dfn.begin(), hi.dfn.end());
    hi.dfn_visited = 0;
    /*
        std::cerr << $(head) << "  " << hi.is_loop_head;
        std::cerr << "  dfn: ";
        for (auto u : hi.dfn) {
          std::cerr << $(u) << " ";
        }
        std::cerr << "  exit: ";
        for (auto u : hi.loop_exit) {
          std::cerr << $(u) << " ";
        }
        std::cerr << '\n';*/
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
    size_t exit_cnt = 0;
    for (auto u : hi.dfn) {
      auto &ui = loop_tree[u];
      for (auto v : u == head ? ui.out : ui.loop_exit) {
        if (v == head) {
          hi.back_edge.push_back(u);
        } else if (idfn.count(v)) {
          assert(idfn[u] < idfn[v]);
          assert(loop_tree[v].loop_head == head);
        } else {
          assert(exit.count(v));
          assert(loop_tree[v].loop_head != head);
          ++exit_cnt;
        }
      }
    }
    assert(!hi.back_edge.empty() == hi.is_loop_head);
    assert(exit_cnt == hi.loop_exit.size());
    hi.loop_exit = std::vector<BB *>(exit.begin(), exit.end());
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
    f.end(w);
  }
  template <class T> void visit_rev(T &f) { _visit_rev(nullptr, f); }
  template <class T> void _visit_rev(BB *w, T &f) {
    auto &wi = loop_tree[w];
    f.begin(w, wi.is_loop_head);
    for (auto u : reverse_view(wi.dfn)) {
      auto &ui = loop_tree[u];
      for (auto v : u == w ? ui.out : ui.loop_exit) {
        auto &vi = loop_tree[v];
        if (v != w && vi.loop_head == w) {
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

struct MergePureCall {
  struct Info {
    std::map<std::pair<Func *, std::vector<Reg>>, Reg> in, out;
    bool visited = 0, is_loop_head = 0;
  };
  ~MergePureCall() {
    if (cnt) {
      ::info << "MergePureCall: " << cnt << '\n';
    }
  }
  std::unordered_map<BB *, Info> info;
  size_t cnt = 0;
  void begin(BB *bb, bool is_loop_head) {
    auto &w = info[bb];
    w.is_loop_head = is_loop_head;
    // std::cerr << bb->name << " : " << is_loop_head << '\n';
  }
  void end(BB *) {}
  void visitBB(BB *bb) {
    // std::cerr << bb->name << " visited" << '\n';
    auto &w = info[bb];
    if (w.is_loop_head) {
      w.out.clear();
    } else {
      w.out = w.in;
    }
    for (auto it = bb->instrs.begin(); it != bb->instrs.end(); ++it) {
      Instr *x = it->get();
      Case(StoreInstr, _, x) {
        w.out.clear();
        (void)_;
      }
      else Case(CallInstr, call, x) {
        if (call->pure) {
          auto key = std::make_pair(call->f, call->args);
          if (w.out.count(key)) {
            *it = std::make_unique<UnaryOpInstr>(call->d1, w.out[key],
                                                 UnaryCompute::ID);
            ++cnt;
            // std::cerr << call->f->name << " merged" << std::endl;
          } else {
            w.out[key] = call->d1;
          }
        } else {
          w.out.clear();
        }
      }
    }
  }
  void visitEdge(BB *bb1, BB *bb2) {
    // std::cerr << bb1->name << " -> " << bb2->name << '\n';
    auto &w1 = info[bb1];
    auto &w2 = info[bb2];
    if (!w2.visited) {
      w2.visited = 1;
      w2.in = w1.out;
    } else {
      decltype(w2.in) tmp;
      for (auto &[k, v] : w2.in) {
        auto it = w1.out.find(k);
        if (it != w1.out.end() && (it->second == v))
          tmp[k] = v;
      }
      w2.in = std::move(tmp);
    }
  }
};

struct LoadToReg {
  struct Info {
    std::map<Reg, Reg> in;
    std::map<Reg, Reg> out;
    bool visited = 0, is_loop_head = 0;
  };
  ~LoadToReg() {
    if (cnt) {
      ::info << "LoadToReg: " << cnt << '\n';
    }
  }
  std::unordered_map<BB *, Info> info;
  size_t cnt = 0;
  void begin(BB *bb, bool is_loop_head) {
    auto &w = info[bb];
    w.is_loop_head = is_loop_head;
    // std::cerr << bb->name << " : " << is_loop_head << '\n';
  }
  void end(BB *) {}
  void visitBB(BB *bb) {
    // std::cerr << bb->name << " visited" << '\n';
    auto &w = info[bb];
    if (w.is_loop_head) {
      w.out.clear();
    } else {
      w.out = w.in;
    }
    for (auto it = bb->instrs.begin(); it != bb->instrs.end(); ++it) {
      Instr *x = it->get();
      Case(LoadInstr, ld, x) {
        if (w.out.count(ld->addr)) {
          *it = std::make_unique<UnaryOpInstr>(ld->d1, w.out[ld->addr],
                                               UnaryCompute::ID);
          ++cnt;
        } else {
          w.out[ld->addr] = ld->d1;
        }
      }
      else Case(StoreInstr, st, x) {
        w.out.clear();
        w.out[st->addr] = st->s1;
      }
      else Case(CallInstr, _, x) {
        (void)_;
        w.out.clear();
      }
    }
  }
  void visitEdge(BB *bb1, BB *bb2) {
    // std::cerr << bb1->name << " -> " << bb2->name << '\n';
    auto &w1 = info[bb1];
    auto &w2 = info[bb2];
    if (!w2.visited) {
      w2.visited = 1;
      w2.in = w1.out;
    } else {
      std::map<Reg, Reg> tmp;
      for (auto [k, v] : w2.in) {
        auto it = w1.out.find(k);
        if (it != w1.out.end() && (it->second == v))
          tmp[k] = v;
      }
      w2.in = std::move(tmp);
    }
  }
};
struct RemoveUnusedStore {
  struct Info {
    std::set<Reg> in;
    std::set<Reg> out;
    bool visited = 0, is_loop_head = 0;
  };
  ~RemoveUnusedStore() {
    if (cnt) {
      ::info << "RemoveUnusedStore: " << cnt << '\n';
    }
  }
  std::unordered_map<BB *, Info> info;
  size_t cnt = 0;
  void begin(BB *bb, bool is_loop_head) {
    auto &w = info[bb];
    w.is_loop_head = is_loop_head;
    // std::cerr << bb->name << " : " << is_loop_head << '\n';
  }
  void end(BB *) {}
  void visitBB(BB *bb) {
    auto &w = info[bb];

    for (auto it = bb->instrs.end(); it != bb->instrs.begin();) {
      auto it0 = it;
      --it;
      Instr *x = it->get();
      Case(LoadInstr, _, x) {
        w.out.clear();
        (void)_;
      }
      else Case(StoreInstr, st, x) {
        if (!w.out.insert(st->addr).second) {
          ++cnt;
          bb->instrs.erase(it);
          it = it0;
        }
      }
      else Case(CallInstr, _, x) {
        w.out.clear();
        (void)_;
      }
    }

    if (w.is_loop_head) {
      w.in.clear();
    } else {
      w.in = w.out;
    }
  }
  void visitEdge(BB *, BB *) {
    // TODO: fix loop exit edges
  }
};

void dag_ir_func(NormalFunc *f) {
  DAG_IR dag(f);
  {
    MergePureCall w;
    dag.visit(w);
  }
  {
    LoadToReg w;
    dag.visit(w);
  }
  {
    RemoveUnusedStore w;
    dag.visit_rev(w);
  }
}
void dag_ir(CompileUnit *ir) { ir->for_each(dag_ir_func); }
