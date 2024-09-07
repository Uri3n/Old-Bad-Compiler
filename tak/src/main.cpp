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


////////////////////////////////////////////////////////////////////////////////////////////////////
// ~ Signal / Interrupt Handlers
// - mostly for debugging
// - exits on ctrl + c
// - tells us if std::terminate is being called
//

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

////////////////////////////////////////////////////////////////////////////////////////////////////
// ~ Commandline argument checks ~
// - predicates for commandline arguments
// - ensures correctness of some flags.
// - used by tak::argp::Handler during the commandline parsing process.
//

static std::optional<std::string>
job_value_chk(const tak::argp::Argument::Value& arg) {
    const auto contained = std::get<int64_t>(arg);
    if(contained < 6 && contained > 0) {
        return std::nullopt;
    }
    return tak::fmt("Job value {} is not valid, must be between 0 and 6.", contained);
}

static std::optional<std::string>
log_lvl_chk(const tak::argp::Argument::Value& arg) {
    const auto contained = std::get<int64_t>(arg);
    if(contained < 4 && contained > -1) {
        return std::nullopt;
    }
    return tak::fmt("Log level {} is invalid. Must be 0, 1 or 2.", contained);
}

static std::optional<std::string>
opt_lvl_chk(const tak::argp::Argument::Value& arg) {
    const auto contained = std::get<int64_t>(arg);
    if(contained < 3 && contained > -1) {
        return std::nullopt;
    }
    return tak::fmt("Optimization level {} is invalid. Must be 0, 1 or 2.", contained);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// ~ Compiler Entry Point ~
// Responsible for:
// - parsing commandline arguments
// - initializing global config
// - starting the compilation process
//

int main(const int argc, char** argv) {
    using namespace tak::argp;

    std::signal(SIGINT, handle_kb_interrupt);
    std::set_terminate(handle_uncaught_exception);

    auto handler = Handler::create(argc, argv)
      .add_parameter(Parameter::create<std::string>("--input", "-i", "The input file to be compiled.", true))
      .add_parameter(Parameter::create<std::string>("--output", "-o", "The name of the output file.", true))
      .add_parameter(Parameter::create<std::string>("--arch", "-a", "A valid LLVM target triple that specifies the target platform."))
      .add_parameter(Parameter::create<Argument::Empty>("--warnings-as-errors", "-werror", "All warnings are treated as errors."))
      .add_parameter(Parameter::create<Argument::Empty>("--dump-ir", "-dumpir", "Dumps the generated LLVM IR to stdout."))
      .add_parameter(Parameter::create<Argument::Empty>("--time-actions", "-ta", "Benchmarks and times compiler actions."))
      .add_parameter(Parameter::create<Argument::Empty>("--dump-ast", "-dumpast", "Dumps the generated Abstract Syntax Tree to stdout."))
      .add_parameter(Parameter::create<Argument::Empty>("--dump-types", "-dumptypes", "Dumps user-defined types (structs) to stdout."))
      .add_parameter(Parameter::create<Argument::Empty>("--dump-symbols", "-dumpsyms", "Dumps the program's Symbol Table to stdout."))
      .add_parameter(Parameter::create<int64_t>("--jobs", "-j", 1, "Source files to be compiled concurrently (max 5).", false, job_value_chk))
      .add_parameter(Parameter::create<int64_t>("--log-level", "-loglvl", 0, "Log level. 0 by default. Can be 0, 1, or 2.", false, log_lvl_chk))
      .add_parameter(Parameter::create<int64_t>("--optimization-level", "-opt", 0, "Optimization level. Can be 0, 1, or 2.", false, opt_lvl_chk));


    if(argc < 2) {
        handler.display_help(R"(Tak "Compiler" (only creates object files for now))");
        return EXIT_SUCCESS;
    }

    handler.parse();
    const uint32_t config_flags = [&]() -> uint32_t {
        uint32_t _flags = tak::CONFIG_FLAGS_NONE;
        if(handler.get<Argument::Empty>("-werror"))        _flags |= tak::CONFIG_WARN_IS_ERR;
        if(handler.get<Argument::Empty>("-dumpir"))        _flags |= tak::CONFIG_DUMP_LLVM;
        if(handler.get<Argument::Empty>("-dumpast"))       _flags |= tak::CONFIG_DUMP_AST;
        if(handler.get<Argument::Empty>("-dumpsyms"))      _flags |= tak::CONFIG_DUMP_SYMBOLS;
        if(handler.get<Argument::Empty>("--time-actions")) _flags |= tak::CONFIG_TIME_ACTIONS;
        if(handler.get<Argument::Empty>("--dump-types"))   _flags |= tak::CONFIG_DUMP_TYPES;
        return _flags;
    }();

    const auto log_lvl = static_cast<tak::log_level_t>(handler.get<int64_t>("--log-level").value());
    const auto opt_lvl = static_cast<tak::opt_level_t>(handler.get<int64_t>("--optimization-level").value());
    const auto jobs    = static_cast<uint16_t>(handler.get<int64_t>("--jobs").value());

    tak::Config::init(
        handler.get<std::string>("--input").value(),
        handler.get<std::string>("--output").value(),
        handler.get<std::string>("--arch"),
        opt_lvl,
        log_lvl,
        config_flags,
        jobs
    );

    return tak::do_compile()
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}