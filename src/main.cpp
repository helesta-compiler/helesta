#include <cstdio>
#include <string>
#include <fstream>

#include "parser/SysYLexer.h"
#include "parser/SysYParser.h"
#include "ir/ir.hpp"
#include "ast/ast_visitor.hpp"

int main(int argc, char **argv) {

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return -1;
    }

    auto filename = std::string(argv[1]);
    std::ifstream source(filename);

    antlr4::ANTLRInputStream input(source);
    SysYLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    SysYParser parser(&tokens);    

    auto root = parser.compUnit();

    IR::CompileUnit ir;
    ASTVisitor visitor(ir);
    auto found_main = std::any_cast<bool>(visitor.visitCompUnit(root));

    printf("found_main: %d\n", found_main);

    return 0;
}
