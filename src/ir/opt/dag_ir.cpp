#include "ir/opt/dag_ir.hpp"

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
