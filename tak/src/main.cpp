#include <token.hpp>
#include <io.hpp>
#include <lexer.hpp>
#include <parser.hpp>

#define CURRENT_TEST "tests/test1.txt"

struct foo {
    int x;
    int y;
};

int main() {

    ///////////////////////////////////////////////////////////////

    /*
     *
     *      (mul)
     *     /    \
     *    3     (add)
     *          /   \
     *         4     1
     */


    int x = (3 * 4) + 1;
    //int y = (10)(123)  <- checks for ; after the first bracket. doesnt find it which means illegal

    /*
     * parse_binary_expression calls parse_expression on "4". That
     * finishes, parse_expression clears the terminal character, in this case ")",
     * at which point parse_binary_expression checks the current token and sees
     * that it's still a binary operator. So it calls parse_expression again (on "1" this time). Repeat
     * this cycle until the entire expression is parsed I think....
     */
    //////////////////////////////////////////////////////////////


#if 1
    lexer lxr;
    if(!lxr.init(CURRENT_TEST)) {
        return 1;
    }

    do {
        lxr.advance(1);
        lexer_display_token_data(lxr.current());
    } while(lxr.current() != END_OF_FILE && lxr.current() != ILLEGAL);

    return EXIT_SUCCESS;
#endif
}