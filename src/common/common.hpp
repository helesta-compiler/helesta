#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using MemSize = uint64_t;

constexpr MemSize INT_SIZE = 4;

enum class ScalarType {
  Void,
  Int,
  Float,
  Char,
};

std::ostream &operator<<(std::ostream &os, const ScalarType &rhs);

int32_t concat(int32_t bottom, int32_t top);

bool startswith(const std::string &s1, const std::string &s2);

// parse [0-9]+ | '0x'[0-9a-fA-F]+ | '0X'[0-9a-fA-F]+
int32_t parse_int32_literal(const std::string &s);
float parse_float_literal(const std::string &s);

std::string mangle_global_var_name(const std::string &s);

struct Configuration {
  static constexpr int DEBUG = 0, INFO = 1, WARNING = 2, ERROR = 3;
  int log_level;
  bool simulate_exec = 0, output_ir = 0;
  std::string input;
  std::set<std::string> disabled_passes;
  std::map<std::string, std::string> args;
  bool give_up;

  Configuration(); // for default setting
  std::string get_arg(std::string key, std::string default_value);
  int get_int_arg(std::string key, int default_value);
};

#define PassEnabled(name) if (!global_config.disabled_passes.count(name))
#define PassDisabled(name) if (global_config.disabled_passes.count(name))

template <typename NodeType> struct Traversable {
  virtual const std::vector<NodeType *> getOutNodes() const = 0;
  virtual void addOutNode(NodeType *node) = 0;
  virtual ~Traversable() = default;
};

template <typename NodeType> struct TreeNode {
  virtual NodeType *getFather() const = 0;
  virtual ~TreeNode() = default;
};

template <typename NodeType>
void construct_outs_for_tree(std::vector<std::unique_ptr<NodeType>> &nodes) {
  for (auto &node : nodes) {
    if (auto fa = node->getFather()) {
      fa->addOutNode(node.get());
    }
  }
}

template <typename NodeSrc, typename NodeDst>
std::vector<std::unique_ptr<NodeDst>>
transfer_graph(const std::vector<std::unique_ptr<NodeSrc>> &srcs) {
  std::unordered_map<NodeSrc *, NodeDst *> src2dst;
  std::vector<std::unique_ptr<NodeDst>> dsts;
  dsts.reserve(srcs.size());
  for (auto &src : srcs) {
    dsts.push_back(std::make_unique<NodeDst>(src.get()));
    src2dst.insert({src.get(), dsts.back().get()});
  }
  for (auto &src : srcs) {
    auto dst = src2dst[src.get()];
    auto outs = src->getOutNodes();
    for (auto out : outs) {
      dst->addOutNode(src2dst[out]);
    }
  }
  return dsts;
};

extern Configuration global_config;

// set global_config, return <input file, output file>
std::pair<std::string, std::string> parse_arg(int argc, char *argv[]);

template <int level> struct LogStream {
  template <class T> LogStream &operator<<(const T &msg) {
    if (global_config.log_level <= level)
      std::cerr << msg;
    return *this;
  }
};

extern LogStream<Configuration::DEBUG> debug;
extern LogStream<Configuration::INFO> info;
extern LogStream<Configuration::WARNING> warning;
extern LogStream<Configuration::ERROR> error;

#ifdef assert
#undef assert
#endif
#define assert(x) ___assert(__LINE__, x, #x, __FILE__)
#define _throw ___assert(__LINE__, 0, "throw", __FILE__), __or ||
void ___assert(int lineno, bool value, const char *expr, const char *file);
struct __or_t {
  template <typename T> void operator||(const T &) {}
};
extern __or_t __or;
#define unreachable() ___assert(__LINE__, 0, "unreachable", __FILE__)

template <class T> struct reverse_view {
  T &x;
  reverse_view(T &_x) : x(_x) {}
  auto begin() { return x.rbegin(); }
  auto end() { return x.rend(); }
};
template <class T> struct enumerate {
  T &x;
  enumerate(T &_x) : x(_x) {}
  struct iterator {
    typename T::iterator x;
    size_t y;
    bool operator!=(const iterator &it) const { return x != it.x; }
    std::pair<decltype(*x), size_t> operator*() const { return {*x, y}; }
    void operator++() {
      ++x;
      ++y;
    }
  };
  auto begin() { return iterator{x.begin(), 0}; }
  auto end() { return iterator{x.end(), x.size()}; }
};

template <class T> struct UnionFind {
  std::unordered_map<T, T> _f;
  T &f(T x) {
    if (_f.count(x))
      return _f[x];
    _f[x] = x;
    return _f[x];
  }
  void add(T x) { _f[x] = x; }
  T operator[](T x) {
    while (x != f(x))
      x = f(x) = f(f(x));
    return x;
  }
  void merge(T x, T y) { f((*this)[x]) = (*this)[y]; }
};

template <class T, class F> void remove_if(T &ls, F f) {
  for (auto it = ls.end(); it != ls.begin();) {
    auto it0 = it;
    if (f(*--it)) {
      ls.erase(it);
      it = it0;
    }
  }
}
template <class T, class F> void remove_if_vec(T &ls, F f) {
  ls.resize(std::remove_if(ls.begin(), ls.end(), f) - ls.begin());
}

template <class Out, class T>
Out &operator<<(Out &out, std::optional<T> const &v) {
  if (v)
    out << "optional(" << *v << ')';
  else
    out << "nullopt";
  return out;
}

inline void _dbg1() {}
template <class T1, class... T2> void _dbg1(const T1 &x, const T2 &...xs) {
  if (global_config.log_level > 1)
    return;
  std::cerr << x;
  _dbg1(xs...);
}
#define dbg(...) _dbg1(__VA_ARGS__)
