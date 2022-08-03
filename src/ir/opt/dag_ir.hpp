#pragma once
#include "ir/opt/dom.hpp"
#include "ir/opt/opt.hpp"
using namespace IR;

inline void _dbg1() {}
template <class T1, class... T2> void _dbg1(const T1 &x, const T2 &... xs) {
  if (global_config.log_level > 0)
    return;
  std::cerr << x;
  _dbg1(xs...);
}
#define dbg(...) _dbg1(__VA_ARGS__)

inline void print_cfg(NormalFunc *f) {
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

inline std::string $(BB *w) { return w ? w->name : "(null)"; }

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
  void rev_traverse(BB *w);
  void traverse(BB *w);
  void find_loop(BB *w, BB *head);
  void _build_dag_dfn(BB *w, BB *head, BB *prev = nullptr);
  void build_dag_dfn(BB *w, BB *head);
  void print(BB *head);
  size_t check_cnt = 0;
  void check_dag(BB *head);
  DAG_IR(NormalFunc *_func);
  template <class T> void visit(T &f) { _visit(nullptr, f); }
  template <class T> void _visit(BB *w, T &f) {
    auto &wi = loop_tree[w];
    f.visitLoopTreeNode(w, &wi);
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
  virtual void visitLoopTreeNode(BB *, DAG_IR::LoopTreeNode *) {}
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
  void visitBB(BB *) {}
  void visitMayExit(BB *, BB *) {}
  void visitEdge(BB *, BB *) {}
  void visitLoopTreeNode(BB *, DAG_IR::LoopTreeNode *) {}
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
inline mem_set_t any_mem{nullptr}, no_mem{};
std::ostream &operator<<(std::ostream &os, const arg_name_t &arg);
std::ostream &operator<<(std::ostream &os, const mem_name_t &ms);
std::ostream &operator<<(std::ostream &os, const mem_set_t &ms);

enum PassType { NORMAL, REMOVE_UNUSED_BB, BEFORE_BACKEND };

struct SideEffect;

struct DAG_IR_ALL {
  CompileUnit *ir;
  std::map<NormalFunc *, std::unique_ptr<DAG_IR>> dags;
  std::map<NormalFunc *, std::unique_ptr<SideEffect>> effect;
  std::map<mem_name_t, mem_set_t> memobjs;
  std::set<std::pair<mem_name_t, arg_name_t>> alias;

  bool typed = 1;
  void update_alias();
  void print();
  void remove_unused_memobj();
  void remove_unused_BB();
  DAG_IR_ALL(CompileUnit *_ir, PassType type);
};

bool type_check(NormalFunc *f);
