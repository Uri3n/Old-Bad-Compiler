#include <exception>
#include <iostream>

#define CURRENT_TEST "tests/test1.txt"

bool do_compile(const std::string& source_file_name);

void
handle_uncaught_exception() {

    const std::exception_ptr exception = std::current_exception();
    try {
        std::rethrow_exception(exception);
    }
    catch(const std::exception& e) {
        std::cerr << "[!] uncaught exception! what: " << e.what() << std::endl;
    }
    catch(...) {
        std::cerr << "[!] unknown fatal exception!" << std::endl;
    }

    std::cerr << "[!] exception routine finished. terminating..." << std::endl;
    std::exit(EXIT_FAILURE);
}


int main() {

    std::set_terminate(handle_uncaught_exception);
    if(!do_compile(CURRENT_TEST)) {
        return EXIT_FAILURE;
    }

    /*
    Lexer lxr;
    if(!lxr.init(CURRENT_TEST)) {
        return EXIT_FAILURE;
    }

    do {
        lxr.advance(1);
        lexer_display_token_data(lxr.current());
    } while(lxr.current() != TOKEN_END_OF_FILE && lxr.current() != TOKEN_ILLEGAL);

    if(lxr.current() == TOKEN_ILLEGAL) {
        lxr.raise_error("Illegal token!", lxr.current().src_pos);
    }
    */

    return EXIT_SUCCESS;
}