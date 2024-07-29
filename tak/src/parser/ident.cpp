//
// Created by Diago on 2024-07-16.
//

#include <parser.hpp>


std::optional<std::string>
get_namespaced_identifier(lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_IDENTIFIER, "Expected identifier.");
    auto full_name = std::string(lxr.current().value);

    while(lxr.peek(1) == TOKEN_NAMESPACE_ACCESS) {
        lxr.advance(2);
        if(lxr.current() != TOKEN_IDENTIFIER) {
            lxr.raise_error("Expected namespace identifier.");
            return std::nullopt;
        }

        full_name += '\\' + std::string(lxr.current().value);
    }

    return full_name;
}


ast_node*
parse_identifier(parser& parser, lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_IDENTIFIER, "Expected identifier.");

    if(lxr.peek(1) == TOKEN_TYPE_ASSIGNMENT || lxr.peek(1) == TOKEN_CONST_TYPE_ASSIGNMENT)
        return parse_decl(parser, lxr);


    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;
    const auto     name     = get_namespaced_identifier(lxr);

    if(!name) return nullptr;

    uint32_t   sym_index      = 0;
    const auto canonical_name = parser.get_canonical_sym_name(*name);


    if(sym_index = parser.lookup_scoped_symbol(canonical_name); sym_index == INVALID_SYMBOL_INDEX) {
        lxr.raise_error("Symbol does not exist in this scope.", curr_pos, line);
        return nullptr;
    }


    auto* ident         = new ast_identifier(); // Otherwise just return a raw identifier node
    ident->symbol_index = sym_index;
    ident->pos          = curr_pos;

    lxr.advance(1);
    return ident;
}