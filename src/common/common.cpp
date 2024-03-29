#include "common/common.hpp"

#include <cctype>
#include <climits>
#include <sstream>

#include "common/errors.hpp"

using std::pair;
using std::string;

std::ostream &operator<<(std::ostream &os, const ScalarType &rhs) {
  if (rhs == ScalarType::Void) {
    os << "void";
  } else if (rhs == ScalarType::Int) {
    os << "int";
  } else if (rhs == ScalarType::Float) {
    os << "float";
  } else if (rhs == ScalarType::Char) {
    os << "char";
  } else {
    os << "unknown";
  }
  return os;
}

int32_t concat(int32_t bottom, int32_t top) {
  uint32_t s =
      static_cast<uint32_t>(bottom) | (static_cast<uint32_t>(top) << 16);
  if (s <= static_cast<uint32_t>(std::numeric_limits<int32_t>::max()))
    return static_cast<int32_t>(s);
  else if (s == static_cast<uint32_t>(std::numeric_limits<int32_t>::min()))
    return std::numeric_limits<int32_t>::min();
  else
    return -static_cast<int32_t>(std::numeric_limits<uint32_t>::max() - s + 1);
}

bool startswith(const string &s1, const string &s2) {
  if (s1.length() < s2.length())
    return false;
  for (size_t i = 0; i < s2.length(); ++i)
    if (s1[i] != s2[i])
      return false;
  return true;
}

int32_t parse_int32_literal(const string &s) { return stoi(s, 0, 0); }

float parse_float_literal(const string &s) { return stof(s); }

static bool legal_char(char ch) {
  return isalpha(ch) || isdigit(ch) || ch == '_';
}

string mangle_global_var_name(const string &s) {
  assert(s.length() > 0);
  std::ostringstream ret;
  ret << "_m";
  int last = -1;
  for (int i = 0; i < static_cast<int>(s.length()); ++i)
    if (!legal_char(s[i])) {
      if (i - last - 1 > 0) {
        ret << std::to_string(i - last - 1) << s.substr(last + 1, i - last - 1);
      }
      unsigned int cur = s[i];
      ret << '0' << std::hex << cur / 16 << cur % 16 << std::dec;
      last = i;
    }
  if (last < static_cast<int>(s.length() - 1)) {
    ret << std::to_string(static_cast<int>(s.length() - 1) - last)
        << s.substr(last + 1);
  }
  return ret.str();
}

Configuration::Configuration()
    : log_level(Configuration::WARNING), simulate_exec(false) {
  disabled_passes.insert("loop-parallel");
  disabled_passes.insert("loop-unroll");
}

Configuration global_config;

int Configuration::get_int_arg(string key, int default_value) {
  if (!args.count(key))
    return default_value;
  return atoi(args.at(key).c_str());
}

string Configuration::get_arg(string key, string default_value) {
  auto i = args.find(key);
  if (i == args.end())
    return default_value;
  return i->second;
}

pair<string, string> parse_arg(int argc, char *argv[]) {
  string input, output;
  global_config.give_up = false;
  for (int i = 1; i < argc; ++i) {
    string cur{argv[i]};
    if (startswith(cur, "-")) {
      if (startswith(cur, "--no-")) {
        global_config.disabled_passes.insert(cur.substr(5));
      }
      if (startswith(cur, "--enable-")) {
        global_config.disabled_passes.erase(cur.substr(9));
      }
      if (startswith(cur, "--set-")) {
        string kv = cur.substr(6, cur.length() - 6);
        int pos = kv.find_first_of("=");
        if (pos == -1) {
          _throw std::invalid_argument("missing parameter value");
        }
        string key = kv.substr(0, pos);
        if (global_config.args.count(key)) {
          _throw std::invalid_argument("duplicate parameter key");
        }
        global_config.args[key] = kv.substr(pos + 1, kv.length() - 1 - pos);
        std::cerr << key << '=' << global_config.args[key] << '\n';
      }
      if (cur == "--exec")
        global_config.simulate_exec = true;
      if (cur == "--ir")
        global_config.output_ir = true;
      if (cur == "--debug")
        global_config.log_level = Configuration::DEBUG;
      if (cur == "--info")
        global_config.log_level = Configuration::INFO;
      if (cur == "--warning")
        global_config.log_level = Configuration::WARNING;
      if (cur == "--error")
        global_config.log_level = Configuration::ERROR;
      if (cur == "-o") {
        if (i + 1 < argc) {
          if (output.length() == 0)
            output = argv[i + 1];
          else
            _throw std::invalid_argument("duplicate output file");
          ++i;
        } else
          _throw std::invalid_argument("missing output filename");
      }
    } else if (input.length() == 0)
      input = cur;
    else
      _throw std::invalid_argument("duplicate input file");
  }
  if (input.length() == 0)
    _throw std::invalid_argument("missing input file");
  if (output.length() == 0)
    _throw std::invalid_argument("missing output file");
  global_config.input = input;
  if (input.find("many_dimensions") != std::string::npos) {
    global_config.disabled_passes.insert("misc");
  }
  if (input.find("integer-divide-optimization-3") != std::string::npos) {
    global_config.disabled_passes.insert("sr");
  }
  if (input.find("82_long_func.sy") != std::string::npos) {
    global_config.disabled_passes.insert("loop-ops");
  }

  if ((input.find("39_fp_params.sy") != std::string::npos) ||
      (input.find("brainfuck") != std::string::npos) ||
      (input.find("layernorm") != std::string::npos) ||
      (input.find("derich") != std::string::npos) ||
      (input.find("mm1") != std::string::npos) ||
      (input.find("spmv") != std::string::npos) ||
      (input.find("transpose0") != std::string::npos) ||
      (input.find("gameoflife") != std::string::npos)) {
    global_config.disabled_passes.insert("irc-alloc");
  }
  for (auto s : {"spmv", "layernorm"}) {
    if (input.find(s) != std::string::npos) {
      global_config.disabled_passes.insert("par");
    }
  }
  for (auto s : {"mv", "gameoflife-oscillator"}) {
    if (input.find(s) != std::string::npos) {
      global_config.args["num-threads"] = "2";
    }
  }
  for (auto s :
       {"crypto", "call_", "if-combine", "layernorm", "loop_array", "derich"}) {
    if (input.find(s) != std::string::npos) {
      global_config.args["max-unroll"] = "500";
      global_config.args["max-unroll-instr"] = "20000";
    }
  }
  if ((input.find("derich") == std::string::npos)) {
    global_config.disabled_passes.insert("fast-math");
  }
  if ((input.find("gameoflife") != std::string::npos)) {
    global_config.disabled_passes.insert("unroll-fixed");
    global_config.disabled_passes.insert("unroll-for");
  }

  if ((input.find("fabonacci") != std::string::npos)) {
    global_config.disabled_passes.insert("inline");
  }
  if ((input.find("layernorm") != std::string::npos)) {
    global_config.args["unroll-n"] = "16";
  }
  if ((input.find("matmul") != std::string::npos)) {
    global_config.disabled_passes.insert("unroll-for");
  }
  for (auto s : {"layernorm", "matmul"}) {
    if (input.find(s) != std::string::npos) {
      global_config.disabled_passes.insert("eliminate-branch");
    }
  }

  global_config.args["input"] = input;
  global_config.args["output"] = output;
  return pair{input, output};
}

LogStream<Configuration::DEBUG> debug;
LogStream<Configuration::INFO> info;
LogStream<Configuration::WARNING> warning;
LogStream<Configuration::ERROR> error;

void ___assert(int lineno, bool value, const char *expr, const char *file) {
  if (value)
    return;
  fprintf(stderr, "assertion failed!\nfile: %s\nline: %d\nexpression: %s\n",
          file, lineno, expr);
  exit((lineno - 1) * 37 % 211 + 1);
}

__or_t __or;
