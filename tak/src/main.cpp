#include <token.hpp>
#include <io.hpp>
#include <lexer.hpp>
#include <parser.hpp>

#define CURRENT_TEST "tests/test2.txt"


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

#else

    lexer lxr;
    if(!lxr.init(CURRENT_TEST)) {
        return EXIT_FAILURE;
    }

    do {
        lxr.advance(1);
        lexer_display_token_data(lxr.current());
    } while(lxr.current() != TOKEN_END_OF_FILE && lxr.current() != TOKEN_ILLEGAL);

#endif

    return EXIT_SUCCESS;
}