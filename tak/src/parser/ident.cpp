//
// Created by Diago on 2024-07-16.
//

#include <parser.hpp>


std::optional<std::string>
tak::get_namespaced_identifier(Lexer& lxr) {

    parser_assert(TOKEN_IDENT_START(lxr.current().type), "Expected identifier.");
    std::string full_name;

    if(lxr.current() == TOKEN_NAMESPACE_ACCESS) {
        if(lxr.peek(1) != TOKEN_IDENTIFIER) {
            lxr.raise_error("Expected identifier after '\\'");
            return std::nullopt;
        }

        lxr.advance(1);
        full_name = '\\' + std::string(lxr.current().value);
    } else {
      full_name = std::string(lxr.current().value);
    }

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


tak::AstNode*
tak::parse_identifier(Parser& parser, Lexer& lxr) {

    parser_assert(TOKEN_IDENT_START(lxr.current().type), "Expected identifier start.");

    if(lxr.peek(1) == TOKEN_TYPE_ASSIGNMENT || lxr.peek(1) == TOKEN_CONST_TYPE_ASSIGNMENT) {
        if(lxr.current() != TOKEN_IDENTIFIER) {
            lxr.raise_error("Illegal token in this context.");
            return nullptr;
        }

        return parse_decl(parser, lxr);
    }


    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;
    const auto     name     = get_namespaced_identifier(lxr);

    if(!name) {
        return nullptr;
    }

    uint32_t   sym_index      = 0;
    const auto canonical_name = parser.get_canonical_sym_name(*name);

    if(sym_index = parser.lookup_scoped_symbol(canonical_name); sym_index == INVALID_SYMBOL_INDEX) {
        sym_index = parser.create_placeholder_symbol(canonical_name, curr_pos, line);
        assert(sym_index != INVALID_SYMBOL_INDEX);
    }

    auto* ident         = new AstIdentifier();
    ident->symbol_index = sym_index;
    ident->pos          = curr_pos;

    lxr.advance(1);
    return ident;
}