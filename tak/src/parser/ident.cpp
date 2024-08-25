//
// Created by Diago on 2024-07-16.
//

#include <parser.hpp>


std::optional<std::string>
tak::get_namespaced_identifier(Lexer& lxr) {

    assert(TOKEN_IDENT_START(lxr.current().type));
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


/*
if(lxr.current() == TOKEN_LSQUARE_BRACKET) {
        if(sym == nullptr) {
            lxr.raise_error("This does not take generic parameters.");
            return nullptr;
        }

        lxr.advance(1);
        std::vector<TypeData> types;
        while(lxr.current() != TOKEN_RSQUARE_BRACKET) {
            if(lxr.current().kind != KIND_TYPE_IDENTIFIER && !TOKEN_IDENT_START(lxr.current().type)) {
                lxr.raise_error("Expected generic type.");
                return nullptr;
            }

            if(const auto type = parse_type(parser, lxr)) {
                types.emplace_back(*type);
            } else {
                return nullptr;
            }

            if(lxr.current() == TOKEN_COMMA || lxr.current() == TOKEN_SEMICOLON) {
                lxr.advance(1);
            }
        }

        if(types.empty()) {
            lxr.raise_error("Empty generic parameters are not allowed.");
            return nullptr;
        }

        auto* new_sym        = parser.tbl_.create_generic_proc_permutation(sym, std::move(types));
        new_sym->file        = lxr.source_file_name_;
        new_sym->line_number = begin_line;
        ident->symbol_index  = new_sym->symbol_index;

        lxr.advance(1);
    }
*/

tak::AstNode*
tak::parse_identifier(Parser& parser, Lexer& lxr) {

    assert(TOKEN_IDENT_START(lxr.current().type));

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

    if(!name) return nullptr;

    uint32_t   sym_index      = 0;
    const auto canonical_name = parser.tbl_.get_canonical_sym_name(*name);

    if(sym_index = parser.tbl_.lookup_scoped_symbol(canonical_name); sym_index == INVALID_SYMBOL_INDEX) {
        sym_index = parser.tbl_.create_placeholder_symbol(canonical_name, lxr.source_file_name_, curr_pos, line);
        assert(sym_index != INVALID_SYMBOL_INDEX);
    }


    bool  state         = false;
    auto* sym           = parser.tbl_.lookup_unique_symbol(sym_index);
    auto* ident         = new AstIdentifier(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);
    ident->symbol_index = sym_index;

    defer_if(!state, [&] {
        delete ident;
    });


    lxr.advance(1);
    if(lxr.current() == TOKEN_DOLLAR_SIGN && lxr.peek(1) == TOKEN_LSQUARE_BRACKET) {
        std::vector<TypeData> types;
        lxr.advance(2);

        while(lxr.current() != TOKEN_RSQUARE_BRACKET) {
            if(lxr.current().kind != KIND_TYPE_IDENTIFIER && !TOKEN_IDENT_START(lxr.current().type)) {
                lxr.raise_error("Expected generic type.");
                return nullptr;
            }

            if(const auto type = parse_type(parser, lxr)) {
                types.emplace_back(*type);
            } else {
                return nullptr;
            }

            if(lxr.current() == TOKEN_COMMA || lxr.current() == TOKEN_SEMICOLON) {
                lxr.advance(1);
            }
        }

        lxr.advance(1);
        if(types.empty()) {
            lxr.raise_error("Empty generic parameters are not allowed.");
            return nullptr;
        }

        auto* new_sym        = parser.tbl_.create_generic_proc_permutation(sym, std::move(types));
        new_sym->file        = lxr.source_file_name_;
        new_sym->line_number = line;
        ident->symbol_index  = new_sym->symbol_index;
    }

    state = true;
    return ident;
}