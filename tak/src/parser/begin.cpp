//
// Created by Diago on 2024-07-04.
//

#include <parser.hpp>


bool
generate_ast_from_source(parser& parser, const std::string& source_file_name) {

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
    return lxr.current() == TOKEN_END_OF_FILE;
}
