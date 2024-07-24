//
// Created by Diago on 2024-07-22.
//

#include <checker.hpp>


bool
check_tree(parser& parser, lexer& lxr) {
    checker_context ctx(lxr, parser);

    for(const auto& decl : parser.toplevel_decls_) {
        visit_node(decl, ctx);
    }

    if(ctx.error_count_ > 0) {
        print("\n{}: BUILD FAILED", lxr.source_file_name_);
        print("Finished with {} errors, {} warnings.", ctx.error_count_, ctx.warning_count_);
        return false;
    }

    return true;
}

// myvar : i32
// myvar + 3

std::optional<type_data>
visit_binexpr(ast_binexpr* node, checker_context& ctx) {

    assert(node != nullptr);

    const auto left_t  = visit_node(node->left_op, ctx);
    const auto right_t = visit_node(node->right_op, ctx);

    if(!left_t || !right_t) {
        ctx.raise_error("Unable to deduce type of one or more operands.", node->pos);
        return std::nullopt;
    }



    return std::nullopt;
}

std::optional<type_data>
visit_unaryexpr(ast_unaryexpr* node, checker_context& ctx) {
    assert(node != nullptr);
    return std::nullopt;
}

std::optional<type_data>
visit_identifier(const ast_identifier* node, const checker_context& ctx) {

    assert(node != nullptr);
    const auto* sym = ctx.parser_.lookup_unique_symbol(node->symbol_index);

    if(sym->type.sym_type == TYPE_KIND_VARIABLE || sym->type.sym_type == TYPE_KIND_PROCEDURE) {
        assert(!node->member_name.has_value()); // Should never be the case if sym != struct
        return sym->type;
    }

    const auto* base_type_name = std::get_if<std::string>(&sym->type.name);
    assert(base_type_name != nullptr);

    if(node->member_name.has_value()) {
        return get_struct_member_type_data(*node->member_name, *base_type_name, ctx.parser_);
    }

    return sym->type;
}

std::optional<type_data>
visit_singleton_literal(ast_singleton_literal* node) {

    assert(node != nullptr);
    type_data data;

    switch(node->literal_type) {
        case TOKEN_STRING_LITERAL:
            data.name          = VAR_I8;
            data.pointer_depth = 1;
            data.flags        |= TYPE_IS_POINTER;
            break;

        case TOKEN_INTEGER_LITERAL:
            data.name = VAR_INTERMEDIATE_UNSIGNED_INTEGRAL;
            break;

        case TOKEN_CHARACTER_LITERAL:
            data.name = VAR_I8;
            break;

        case TOKEN_FLOAT_LITERAL:
            data.name = VAR_INTERMEDIATE_UNSIGNED_FLOAT;
            break;

        case TOKEN_BOOLEAN_LITERAL:
            data.name = VAR_BOOLEAN;
            break;

        default:
            panic("visit_singleton_literal: default case reached.");
    }

    data.flags   |= TYPE_IS_CONSTANT;
    data.sym_type = TYPE_KIND_VARIABLE;
    return { data };
}



std::optional<type_data>
visit_node(ast_node* node, checker_context& ctx) {

    assert(node != nullptr);
    assert(node->type != NODE_NONE);

    switch(node->type) {
        case NODE_VARDECL:
        case NODE_PROCDECL:
        case NODE_BINEXPR:           return visit_binexpr(dynamic_cast<ast_binexpr*>(node), ctx);
        case NODE_UNARYEXPR:         return visit_unaryexpr(dynamic_cast<ast_unaryexpr*>(node), ctx);
        case NODE_SINGLETON_LITERAL: return visit_singleton_literal(dynamic_cast<ast_singleton_literal*>(node));
        case NODE_IDENT:             return visit_identifier(dynamic_cast<ast_identifier*>(node), ctx);
        case NODE_BRANCH:
        case NODE_IF:
        case NODE_ELSE:
        case NODE_FOR:
        case NODE_SWITCH:
        case NODE_CASE:
        case NODE_DEFAULT:
        case NODE_WHILE:
        case NODE_DOWHILE:
        case NODE_BLOCK:
        case NODE_CALL:
        case NODE_RET:
        case NODE_DEFER:
        case NODE_SIZEOF:
        case NODE_BRACED_EXPRESSION:
        case NODE_SUBSCRIPT:
        case NODE_NAMESPACEDECL:
        case NODE_CAST:

        default: break;
    }

    panic("visit_node: invalid AST node passed.");
}



