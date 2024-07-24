//
// Created by Diago on 2024-07-16.
//

#include <parser.hpp>
#include <checker.hpp>

ast_node*
parse_member_access(const uint32_t sym_index, parser& parser, lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_DOT, "Expected '.'");

    const auto* sym = parser.lookup_unique_symbol(sym_index);
    if(sym == nullptr) {
        return nullptr;
    }


    //
    // Check struct type.
    //

    const auto* type_name = std::get_if<std::string>(&sym->type.name);
    if(type_name == nullptr || sym->type.sym_type != TYPE_KIND_STRUCT) {
        lxr.raise_error("Attempted member access on non-struct type.");
        return nullptr;
    }

    if(!parser.type_exists(*type_name)) {
        lxr.raise_error("Type does not exist.");
        return nullptr;
    }

    const auto* _member_data = parser.lookup_type(*type_name);
    if(_member_data == nullptr) {
        return nullptr;
    }


    //
    // Check if the member exists.
    //

    lxr.advance(1);
    if(lxr.current() != TOKEN_IDENTIFIER) {
        lxr.raise_error("Expected struct member name.");
        return nullptr;
    }


    std::string path;
    auto looking = std::string(lxr.current().value);
    std::function<bool(decltype(_member_data))> get_member_path;


    //
    // Recurse through each sub-member (if they exist) until we find our target
    //

    get_member_path = [&](const decltype(_member_data) members) -> bool {

        if(members == nullptr) {
            lxr.raise_error("Struct does not have any members.");
            return false;
        }

        for(const auto& member : *members) {
            if(member.name != looking)
                continue;

            path += fmt(".{}", looking);
            if(lxr.peek(1) == TOKEN_DOT && lxr.peek(2) == TOKEN_IDENTIFIER) {
                lxr.advance(2);
                looking = std::string(lxr.current().value);

                auto* substruct_name = std::get_if<std::string>(&member.type.name);
                if(substruct_name == nullptr || member.type.sym_type != TYPE_KIND_STRUCT) {
                    lxr.raise_error("Attempting to access member from non-struct type.");
                    return false;
                }

                return get_member_path(parser.lookup_type(*substruct_name));
            }

            lxr.advance(1);
            return true;
        }

        lxr.raise_error("Struct member does not exist.");
        return false;
    };

    if(!get_member_path(_member_data)) {
        return nullptr;
    }


    //
    // Generate AST node.
    //

    auto* node         = new ast_identifier();
    node->pos          = lxr.current().src_pos;
    node->member_name  = path;
    node->symbol_index = sym->symbol_index;

    return node;
}


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


    lxr.advance(1);
    if(lxr.current() == TOKEN_DOT)
        return parse_member_access(sym_index, parser, lxr);

    auto* ident         = new ast_identifier(); // Otherwise just return a raw identifier node
    ident->symbol_index = sym_index;
    ident->pos          = curr_pos;

    return ident;
}