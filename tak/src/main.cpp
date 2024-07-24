#include <token.hpp>
#include <io.hpp>
#include <lexer.hpp>
#include <parser.hpp>
#include <checker.hpp>
#include <exception>

#define CURRENT_TEST "tests/test1.txt"


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


void typecomp_test() {

    type_data type1;
    type_data type2;
    type_data type3;
    type_data type4;

    type1.sym_type = TYPE_KIND_STRUCT;
    type1.name = "mystruct";
    type1.pointer_depth = 1;

    type2.sym_type = TYPE_KIND_VARIABLE;
    type2.name = VAR_I64;
    type2.pointer_depth = 0;

    type3.sym_type = TYPE_KIND_PROCEDURE;
    type3.name = std::monostate();
    type3.return_type = std::make_shared<type_data>(type1);
    type3.parameters = std::make_shared<std::vector<type_data>>();
    type3.parameters->emplace_back(type1);
    type3.parameters->emplace_back(type1);

    type4.sym_type = TYPE_KIND_PROCEDURE;
    type4.name = std::monostate();
    type4.return_type = std::make_shared<type_data>(type1);
    type4.parameters = std::make_shared<std::vector<type_data>>();
    type4.parameters->emplace_back(type2);


    if(types_are_identical(type3, type4)) {
        print("Types are identical!");
    } else {
        print("Types are not the same!");
    }
}

int main() {

    std::set_terminate(handle_uncaught_exception);

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

   if(lxr.current() == TOKEN_ILLEGAL) {
       lxr.raise_error("This is an error!!", lxr.current().src_pos);
   }

#endif

    return EXIT_SUCCESS;
}