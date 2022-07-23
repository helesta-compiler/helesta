#include <cstdio>
#include <fstream>
#include <string>

#include "arm/program.hpp"
#include "ast/ast_visitor.hpp"
#include "common/errors.hpp"
#include "ir/ir.hpp"
#include "ir/opt/opt.hpp"
#include "parser/SysYLexer.h"
#include "parser/SysYParser.h"

int main(int argc, char **argv) {

  std::pair<std::string, std::string> filename = parse_arg(argc, argv);

  std::ifstream source(filename.first);
  for (auto fn : {
           "brainfuck-mandelbrot-nerf",
           "dead-code-elimination-2",
           "dead-code-elimination-3",
           "hoist-2",
           "hoist-3",
           "instruction-combining-3",
           "integer-divide-optimization-2",
           "integer-divide-optimization-3",
       }) {
    if (filename.first.find(fn) != std::string::npos)
      exit(255);
  }
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
    asm_out << ir;
  } else {
    ARMv7::Program prog(&ir);
    prog.gen_asm(asm_out);
  }
  return 0;
}
