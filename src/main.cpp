#include <cstdio>
#include <fstream>
#include <string>
#include <sys/resource.h>

#include "arm/program.hpp"
#include "ast/ast_visitor.hpp"
#include "common/errors.hpp"
#include "ir/ir.hpp"
#include "ir/opt/opt.hpp"
#include "parser/SysYLexer.h"
#include "parser/SysYParser.h"

int main(int argc, char **argv) {
  const rlim_t kStackSize = 64L * 1024L * 1024L;
  struct rlimit rl;
  int result;
  result = getrlimit(RLIMIT_STACK, &rl);
  if (result == 0) {
    if (rl.rlim_cur < kStackSize) {
      rl.rlim_cur = kStackSize;
      result = setrlimit(RLIMIT_STACK, &rl);
      if (result != 0)
        std::cerr << "setrlimit failed, use ulimit -s unlimited instead."
                  << std::endl;
    }
  }

  std::pair<std::string, std::string> filename = parse_arg(argc, argv);

  if (global_config.give_up) {
    std::cerr << "give up" << std::endl;
    exit(255);
  }

  std::ifstream source(filename.first);

  antlr4::ANTLRInputStream input(source);
  SysYLexer lexer(&input);
  antlr4::CommonTokenStream tokens(&lexer);
  SysYParser parser(&tokens);

  auto root = parser.compUnit();

  IR::CompileUnit ir;
  ASTVisitor visitor(ir);
  auto found_main = visitor.visitCompUnit(root).as<bool>();
  if (!found_main) {
    _throw MainFuncNotFound();
  }

  optimize_ir(&ir);

  if (global_config.simulate_exec)
    IR::exec(ir);
  std::ofstream asm_out{filename.second};
  if (global_config.output_ir) {
    PassEnabled("ir") asm_out << ir;
  }
  PassEnabled("asm") {
    ARMv7::Program prog(&ir);
    prog.gen_asm(asm_out);
  }
  return 0;
}
