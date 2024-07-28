//
// Created by Diago on 2024-07-22.
//

#include <checker.hpp>


bool
check_tree(parser& parser, lexer& lxr) {

    checker_context ctx(lxr, parser);

    for(const auto& decl : parser.toplevel_decls_) {
        if(NODE_NEEDS_VISITING(decl->type)) {
            visit_node(decl, ctx);
        }
    }

    if(ctx.error_count_ > 0) {
        print("\n{}: BUILD FAILED", lxr.source_file_name_);
        print("Finished with {} errors, {} warnings.", ctx.error_count_, ctx.warning_count_);
        return false;
    }

    return true;
}


std::optional<type_data>
visit_binexpr(ast_binexpr* node, checker_context& ctx) {

    assert(node != nullptr);

    auto  left_t = visit_node(node->left_op, ctx);
    auto right_t = visit_node(node->right_op, ctx);

    if(!left_t || !right_t) {
        ctx.raise_error("Unable to deduce type of one or more operands.", node->pos);
        return std::nullopt;
    }


    const auto     op_as_str    = lexer_token_to_string(node->_operator);
    const auto     left_as_str  = format_type_data_for_string_msg(*left_t);
    const auto     right_as_str = format_type_data_for_string_msg(*right_t);
    const uint32_t curr_errs    = ctx.error_count_;


    if(TOKEN_OP_IS_LOGICAL(node->_operator)) {
        if(!can_operator_be_applied_to(node->_operator, *left_t)) {
            ctx.raise_error(fmt("Logical operator '{}' cannot be applied to lefthand type {}.", op_as_str, left_as_str), node->pos);
        }

        if(!can_operator_be_applied_to(node->_operator, *right_t)) {
            ctx.raise_error(fmt("Logical operator '{}' cannot be applied to righthand type {}.", op_as_str, right_as_str), node->pos);
        }

        type_data constbool;
        constbool.name   = VAR_BOOLEAN;
        constbool.kind   = TYPE_KIND_VARIABLE;
        constbool.flags  = TYPE_IS_CONSTANT | TYPE_RVALUE;

        return constbool;
    }


    if(!is_type_coercion_permissible(*left_t, *right_t)) {
        ctx.raise_error(fmt("Cannot coerce type of righthand expression ({}) to {}", right_as_str, left_as_str), node->pos);
    }

    if(!can_operator_be_applied_to(node->_operator, *left_t)) {
        ctx.raise_error(fmt("Operator '{}' cannot be applied to lefthand type {}.", op_as_str, left_as_str), node->pos);
    }

    if(curr_errs != ctx.error_count_) {
        return std::nullopt;
    }

    left_t->flags |= TYPE_IS_CONSTANT | TYPE_RVALUE;
    return *left_t;
}


std::optional<type_data>
visit_unaryexpr(ast_unaryexpr* node, checker_context& ctx) {

    assert(node != nullptr);
    assert(node->operand != nullptr);

    auto operand_t = visit_node(node->operand, ctx);
    if(!operand_t) {
        return std::nullopt;
    }

    const auto op_as_str   = lexer_token_to_string(node->_operator);
    const auto type_as_str = format_type_data_for_string_msg(*operand_t);


    if(node->_operator == TOKEN_SUB || node->_operator == TOKEN_PLUS) {
        if(operand_t->flags & TYPE_IS_POINTER || !operand_t->array_lengths.empty() || operand_t->kind != TYPE_KIND_VARIABLE) {
            ctx.raise_error(fmt("Cannot apply unary operator {} to type {}.", op_as_str, type_as_str), node->pos);
            return std::nullopt;
        }

        if(node->_operator == TOKEN_SUB) {
            if(!flip_sign(*operand_t)) {
                ctx.raise_error(fmt("Cannot apply unary minus to type {}.", op_as_str, type_as_str), node->pos);
                return std::nullopt;
            }
        }

        operand_t->flags |= TYPE_IS_CONSTANT | TYPE_RVALUE;
        return operand_t;
    }

    if(node->_operator == TOKEN_BITWISE_NOT) {
        if(!is_type_bwop_eligible(*operand_t)) {
            ctx.raise_error(fmt("Cannot apply bitwise operator ~ to type {}", type_as_str), node->pos);
            return std::nullopt;
        }

        operand_t->flags |= TYPE_IS_CONSTANT | TYPE_RVALUE;
        return operand_t;
    }

    if(node->_operator == TOKEN_INCREMENT || node->_operator == TOKEN_DECREMENT) {
        if(!can_operator_be_applied_to(node->_operator, *operand_t)) {
            ctx.raise_error(fmt("Cannot apply operator {} to type {}", op_as_str, type_as_str), node->pos);
            return std::nullopt;
        }

        operand_t->flags |= TYPE_IS_CONSTANT | TYPE_RVALUE;
        return operand_t;
    }

    if(node->_operator == TOKEN_CONDITIONAL_NOT) {
        if(!is_type_lop_eligible(*operand_t)) {
            ctx.raise_error(fmt("Cannot apply logical operator ! to type {}", type_as_str), node->pos);
            return std::nullopt;
        }

        type_data constbool;
        constbool.name   = VAR_BOOLEAN;
        constbool.kind   = TYPE_KIND_VARIABLE;
        constbool.flags  = TYPE_IS_CONSTANT | TYPE_RVALUE;

        return constbool;
    }

    if(node->_operator == TOKEN_BITWISE_XOR_OR_PTR) {
        if(const auto deref_t = get_dereferenced_type(*operand_t)) {
            return deref_t;
        }

        ctx.raise_error(fmt("Cannot dereference type {}.", type_as_str), node->pos);
        return std::nullopt;
    }

    if(node->_operator == TOKEN_BITWISE_AND) {
        if(const auto addressed_t = get_addressed_type(*operand_t)) {
            return addressed_t;
        }

        ctx.raise_error("Operand cannot be addressed.", node->pos);
        return std::nullopt;
    }


    panic("visit_unaryexpr: no suitable operator found.");
}


std::optional<type_data>
visit_identifier(const ast_identifier* node, const checker_context& ctx) {

    assert(node != nullptr);
    const auto* sym = ctx.parser_.lookup_unique_symbol(node->symbol_index);

    if(sym->type.kind == TYPE_KIND_VARIABLE || sym->type.kind == TYPE_KIND_PROCEDURE) {
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
visit_singleton_literal(ast_singleton_literal* node, checker_context& ctx) {

    assert(node != nullptr);
    type_data data;

    switch(node->literal_type) {
        case TOKEN_STRING_LITERAL:
            data.name          = VAR_I8;
            data.pointer_depth = 1;
            data.flags        |= TYPE_IS_POINTER;
            break;

        case TOKEN_FLOAT_LITERAL:
            data = convert_float_lit_to_type(node);
            break;

        case TOKEN_INTEGER_LITERAL:
            data = convert_int_lit_to_type(node);
            break;

        case TOKEN_CHARACTER_LITERAL:
            data.name = VAR_I8;
            break;

        case TOKEN_BOOLEAN_LITERAL:
            data.name = VAR_BOOLEAN;
            break;

        default:
            panic("visit_singleton_literal: default case reached.");
    }

    data.flags |= TYPE_IS_CONSTANT | TYPE_RVALUE;
    data.kind   = TYPE_KIND_VARIABLE;
    return { data };
}


std::optional<type_data>
visit_vardecl(const ast_vardecl* node, checker_context& ctx) {

    assert(node != nullptr);
    assert(node->identifier != nullptr);
    assert(!node->identifier->member_name);

    auto* sym = ctx.parser_.lookup_unique_symbol(node->identifier->symbol_index);
    assert(sym != nullptr);

    if(node->init_value) {
        const auto init_t = visit_node(*node->init_value, ctx);
        if(!init_t) {
            return std::nullopt;
        }

        if(!is_type_coercion_permissible(sym->type, *init_t)) {
            ctx.raise_error(fmt("Cannot assign variable {} of type {} to {}.",
                sym->name,
                format_type_data_for_string_msg(sym->type),
                format_type_data_for_string_msg(*init_t)),
                node->pos
            );

            return std::nullopt;
        }
    }

    return sym->type;
}


std::optional<type_data>
visit_cast(const ast_cast* node, checker_context& ctx) {

    assert(node != nullptr);
    const auto  target_t = visit_node(node->target, ctx);
    const auto& cast_t   = node->type;

    if(!target_t) {
        return std::nullopt;
    }

    const std::string target_t_str = format_type_data_for_string_msg(*target_t);
    const std::string cast_t_str   = format_type_data_for_string_msg(cast_t);

    if(!is_type_cast_permissible(*target_t, cast_t)) {
        ctx.raise_error(fmt("Cannot cast type {} to {}.", target_t_str, cast_t_str), node->pos);
        return std::nullopt;
    }

    return { cast_t };
}


std::optional<type_data>
visit_node(ast_node* node, checker_context& ctx) {

    assert(node != nullptr);
    assert(node->type != NODE_NONE);

    switch(node->type) {
        case NODE_VARDECL:           return visit_vardecl(dynamic_cast<ast_vardecl*>(node), ctx);
        case NODE_PROCDECL:          break;
        case NODE_BINEXPR:           return visit_binexpr(dynamic_cast<ast_binexpr*>(node), ctx);
        case NODE_UNARYEXPR:         return visit_unaryexpr(dynamic_cast<ast_unaryexpr*>(node), ctx);
        case NODE_SINGLETON_LITERAL: return visit_singleton_literal(dynamic_cast<ast_singleton_literal*>(node), ctx);
        case NODE_IDENT:             return visit_identifier(dynamic_cast<ast_identifier*>(node), ctx);
        case NODE_CAST:              return visit_cast(dynamic_cast<ast_cast*>(node), ctx);
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

        default: break;
    }

    panic("visit_node: invalid AST node passed.");
}



