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

    if(parser.scope_stack.empty()) {
        parser.push_scope(); // push global scope
    }


    do {
        toplevel_decl = parse_expression(parser, lxr, false);
        if(toplevel_decl == nullptr) {
            break;
        }

        parser.toplevel_decls.emplace_back(toplevel_decl);
    } while(true);


    parser.pop_scope();
    return lxr.current() == END_OF_FILE;
}
