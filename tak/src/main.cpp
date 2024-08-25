#include <exception>
#include <iostream>
#include <io.hpp>
#include <argparse.hpp>

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

// So that I can test shit when i need to. This function is useless.
static void cmdline_argtest() {
    using namespace tak;

    std::vector<std::string> chunks = { "-m", "ass", "--intparm", "42069"};
    auto param1 = argp::Parameter::create<std::string>("--myparm", "-m", "This is a poopy doopie parameter.", true);
    auto param2 = argp::Parameter::create<int64_t>("--intparm", "-ip", "if this param is set ur gay", true);

    std::optional<std::string> result1;
    std::optional<int64_t>     result2;

    auto handler = argp::Handler::create(std::move(chunks))
        .add_parameter(std::move(param1))
        .add_parameter(std::move(param2))
        .parse();

    handler
        .get("--myparm", result1)
        .get("--intparm", result2);

    if(result1) print("Result 1: {}", *result1);
    if(result2) print("Result 2: {}", *result2);

    handler.display_help();
}

int main(int argc, char** argv) {
    std::set_terminate(handle_uncaught_exception);
    if(!do_compile(CURRENT_TEST)) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}