#include <exception>
#include <iostream>
#include <io.hpp>
#include <argparse.hpp>
#include <config.hpp>
#include <csignal>

#ifdef TAK_DEBUG
#define CURRENT_TEST "tests/test1.txt"
#endif

namespace tak {
    bool do_compile();
}

static void
handle_uncaught_exception() {
    using namespace tak;
    const std::exception_ptr exception = std::current_exception();

    try {
        std::rethrow_exception(exception);
    }
    catch(const std::exception& e) {
        red_bold("\n[!] uncaught fatal exception! what: {}", e.what());
    }
    catch(...) {
        red_bold("\n[!] unknown fatal exception!");
    }

    red_bold("[!] exception routine finished. terminating...");
    exit(EXIT_FAILURE);
}

static void
handle_kb_interrupt(const int signal) {
    using namespace tak;
    bold("\nKeyboard interrupt recieved (signal {})", signal);
    bold("Exiting...");
    exit(EXIT_FAILURE);
}

int main(const int argc, char** argv) {
    using namespace tak::argp;

    std::signal(SIGINT, handle_kb_interrupt);
    std::set_terminate(handle_uncaught_exception);

    if(argc < 3) {
        tak::print("USAGE: [INPUT] [OUTPUT]");
        tak::print("Note: this barely works.");
        return 0;
    }

    tak::Config::init(
        argv[1],
        argv[2],
        std::nullopt,
        tak::OPTIMIZATION_LEVEL_00,
        tak::LOG_LEVEL_ENABLED,
        tak::CONFIG_DUMP_AST
            | tak::CONFIG_DUMP_LLVM
            | tak::CONFIG_DUMP_TYPES
            | tak::CONFIG_DUMP_SYMBOLS,
        1
    );

    return tak::do_compile() ? 0 : 1;
}