#include <exception>
#include <iostream>
#include <io.hpp>

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

    return EXIT_SUCCESS;
}