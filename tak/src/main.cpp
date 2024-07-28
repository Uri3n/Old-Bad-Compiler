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

    type1.kind = TYPE_KIND_VARIABLE;
    type1.name = VAR_F64;

    type2.kind = TYPE_KIND_VARIABLE;
    type2.name = VAR_I64;
    type2.flags |= TYPE_IS_POINTER;
    type2.pointer_depth = 1;

    type3.kind = TYPE_KIND_PROCEDURE;
    type3.name = std::monostate();
    type3.return_type = std::make_shared<type_data>(type1);
    type3.parameters = std::make_shared<std::vector<type_data>>();
    type3.parameters->emplace_back(type1);
    type3.parameters->emplace_back(type1);

    type4.kind = TYPE_KIND_PROCEDURE;
    type4.name = std::monostate();
    type4.return_type = std::make_shared<type_data>(type1);
    type4.parameters = std::make_shared<std::vector<type_data>>();
    type4.parameters->emplace_back(type2);


    if(is_type_coercion_permissible(type1, type2)) {
        print("right type is coercible to left type!");
    } else {
        print("Type coercion not permitted!");
    }
}


static bool
generate_check_ast(parser& parser, const std::string& source_file_name) {

    lexer     lxr;
    ast_node* toplevel_decl = nullptr;


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
    //parser.dump_symbols();
    //parser.dump_types();
    return check_tree(parser, lxr);
}

int main() {

    std::set_terminate(handle_uncaught_exception);

#if 1

    parser parser;

    if(!generate_check_ast(parser, CURRENT_TEST)) {
        print("Failed to parse file.");
        return EXIT_FAILURE;
    }

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