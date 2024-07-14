#include <token.hpp>
#include <io.hpp>
#include <lexer.hpp>
#include <parser.hpp>

#define CURRENT_TEST "tests/test1.txt"



int main() {

#if 1
    parser parser;

    if(!generate_ast_from_source(parser, CURRENT_TEST)) {
        print("Failed to parse file.");
        return EXIT_FAILURE;
    }

    parser.dump_nodes();
    parser.dump_symbols();
    parser.dump_types();
#endif

    return EXIT_SUCCESS;
}