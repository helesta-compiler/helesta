#include <cstdio>
#include <fstream>
#include <string>

#include "backend_passes.hpp"
#include "program.hpp"
#include "ast_visitor.hpp"
#include "errors.hpp"
#include "ir.hpp"
#include "pass.hpp"
#include "SysYLexer.h"
#include "SysYParser.h"

int main(int argc, char **argv) {

  std::pair<std::string, std::string> filename = parse_arg(argc, argv);

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
    throw MainFuncNotFound();
  }

  ARMv7::Program prog(&ir);
  ARMv7::optimize_before_reg_alloc(&prog);
  std::ofstream asm_out{filename.second};
  prog.gen_asm(asm_out);

  return 0;
}
