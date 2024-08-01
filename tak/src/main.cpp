#include <token.hpp>
#include <io.hpp>
#include <lexer.hpp>
#include <parser.hpp>
#include <checker.hpp>
#include <exception>

#define CURRENT_TEST "tests/test_utf8.txt"


void
handle_uncaught_exception() {

    const std::exception_ptr exception = std::current_exception();

    try {
        std::rethrow_exception(exception);
    }
    catch(const std::exception& e) {
        std::cerr << "uncaught exception! what: " << e.what() << std::endl;
    }
    catch(...) {
        std::cerr << "unknown fatal exception!" << std::endl;
    }

    std::cerr << "[!] exception routine finished. terminating..." << std::endl;
    std::exit(EXIT_FAILURE);
}


static bool
generate_check_ast(Parser& parser, const std::string& source_file_name) {

    Lexer     lxr;
    AstNode* toplevel_decl = nullptr;


    if(!lxr.init(source_file_name)) {
        return false;
    }

    if(parser.scope_stack_.empty()) {
        parser.push_scope(); // push global scope
    }

    do {
        toplevel_decl = parse_expression(parser, lxr, false);
        if(toplevel_decl == nullptr) {
            break;
        }

        /*
        if(!VALID_AT_TOPLEVEL(toplevel_decl->type)) {
            lxr.raise_error("Invalid toplevel statement or expression.");
            return false;
        }*/

        parser.toplevel_decls_.emplace_back(toplevel_decl);
    } while(true);

    parser.pop_scope();
    if(lxr.current() != TOKEN_END_OF_FILE) {
        return false;
    }

    parser.dump_nodes();
    parser.dump_types();

    if(!visit_tree(parser, lxr) ) {
        return false;
    }

    parser.dump_symbols();
    return true;
}

int main() {

    std::set_terminate(handle_uncaught_exception);

#if 1

    Parser parser;

    if(!generate_check_ast(parser, CURRENT_TEST)) {
        print("Failed to parse file.");
        return EXIT_FAILURE;
    }

#else

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

#endif

    return EXIT_SUCCESS;
}