//
// Created by Diago on 2024-07-16.
//

#include <parser.hpp>




tak::AstNode*
tak::parse_namespace(Parser& parser, Lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_KW_NAMESPACE, "Expected \"namespace\" keyword.");
    lxr.advance(1);

    const size_t   begin_pos  = lxr.current().src_pos;
    const uint32_t begin_line = lxr.current().line;


    if(parser.tbl_.scope_stack_.size() > 1) {
        lxr.raise_error("Namespace declaration at non-global scope.");
        return nullptr;
    }

    if(lxr.current() != TOKEN_IDENTIFIER) {
        lxr.raise_error("Expected namespace identifier.");
        return nullptr;
    }

    const auto namespace_str = get_namespaced_identifier(lxr);
    if(!namespace_str) {
        return nullptr;
    }

    const auto split = split_string(*namespace_str, '\\');
    for(const auto& _namespace : split) {
       if(!parser.tbl_.enter_namespace(_namespace)) {
           lxr.raise_error(fmt("Nested namespace {} has the same name as a parent.", _namespace), begin_pos, begin_line);
           return nullptr;
       }
    }


    bool  once      = false;
    bool  state     = false;
    auto* node      = new AstNamespaceDecl(begin_pos, begin_line, lxr.source_file_name_);
    node->full_path = parser.tbl_.namespace_as_string();

    defer([&] {
        if(!state) { delete node; }
        for(size_t i = 0; i < split.size(); i++) parser.tbl_.leave_namespace();
    });


    lxr.advance(1);
    if(lxr.current() != TOKEN_LBRACE) {
        once = true;
    } else {
        lxr.advance(1);
    }

    while(lxr.current() != TOKEN_RBRACE || once) {

        const size_t   curr_pos  = lxr.current().src_pos;
        const uint32_t curr_line = lxr.current().line;

        node->children.emplace_back(parse_expression(parser, lxr, false));
        if(node->children.back() == nullptr)
            return nullptr;

        if(!NODE_VALID_AT_TOPLEVEL(node->children.back()->type)) {
            lxr.raise_error("Expression is invalid as a toplevel statement.", curr_pos, curr_line);
            return nullptr;
        }

        node->children.back()->parent = node;
        if(once) {
            state = true;
            return node;
        }
    }

    lxr.advance(1);
    state = true;
    return node;
}
