#include <token.hpp>
#include <io.hpp>
#include <lexer.hpp>
#include <parser.hpp>

#define CURRENT_TEST "tests/test1.txt"


int main() {

    lexer lxr;
    if(!lxr.init(CURRENT_TEST)) {
        return 1;
    }

    do {
        lxr.advance(1);
        lexer_display_token_data(lxr.current());
    } while(lxr.current() != END_OF_FILE && lxr.current() != ILLEGAL);

    return EXIT_SUCCESS;
}