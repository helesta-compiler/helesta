#include <cstdio>
#include <string>
#include <fstream>

#include "frontend/SysYLexer.h"
#include "frontend/SysYParser.h"

int main(int argc, char **argv) {

    if (argc != 2) {
        fprintf(stderr, "Usage: ./%s <file>\n", argv[0]);
        return -1;
    }

    auto filename = std::string(argv[1]);
    std::ifstream source(filename);

    antlr4::ANTLRInputStream input(source);
    SysYLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    SysYParser parser(&tokens);    

    auto root = parser.compUnit();

    return 0;
}
