#include <cstdio>
#include <string>
#include <fstream>

#include "common/errors.hpp"
#include "parser/SysYLexer.h"
#include "parser/SysYParser.h"
#include "ir/ir.hpp"
#include "ast/ast_visitor.hpp"
#include "arm/backend_passes.hpp"
#include "arm/program.hpp"

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
    auto found_main = std::any_cast<bool>(visitor.visitCompUnit(root));
    if (!found_main) {
        throw MainFuncNotFound();
    }

    ARMv7::Program prog(&ir);
    ARMv7::optimize_before_reg_alloc(&prog);
    std::ofstream asm_out{filename.second};
    prog.gen_asm(asm_out);

    return 0;
}
