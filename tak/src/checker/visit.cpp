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


    // edge situation : braced expr being assigned to struct.
    if(left_t.has_value()
        && left_t->kind         == TYPE_KIND_STRUCT
        && node->right_op->type == NODE_BRACED_EXPRESSION
        && node->_operator      == TOKEN_VALUE_ASSIGNMENT
        && can_operator_be_applied_to(TOKEN_VALUE_ASSIGNMENT, *left_t)
    ) {
        check_structassign_bracedexpr(*left_t, dynamic_cast<ast_braced_expression*>(node->right_op), ctx);
        left_t->flags |= TYPE_IS_CONSTANT | TYPE_RVALUE;
        return *left_t;
    }

    if(!left_t || !right_t) {
        ctx.raise_error("Unable to deduce type of one or more operands.", node->pos);
        return std::nullopt;
    }


    const auto     op_as_str    = lexer_token_to_string(node->_operator);
    const auto     left_as_str  = typedata_to_str_msg(*left_t);
    const auto     right_as_str = typedata_to_str_msg(*right_t);
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


    if(!can_operator_be_applied_to(node->_operator, *left_t)) {
        ctx.raise_error(fmt("Operator '{}' cannot be applied to lefthand type {}.", op_as_str, left_as_str), node->pos);
    }

    if(!is_type_coercion_permissible(*left_t, *right_t)) {
        ctx.raise_error(fmt("Cannot coerce type of righthand expression ({}) to {}", right_as_str, left_as_str), node->pos);
    }

    if(curr_errs != ctx.error_count_) {
        return std::nullopt;
    }

    left_t->flags |= TYPE_IS_CONSTANT | TYPE_RVALUE;
    return left_t;
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
    const auto type_as_str = typedata_to_str_msg(*operand_t);


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
        return sym->type;
    }

    const auto* base_type_name = std::get_if<std::string>(&sym->type.name);
    assert(base_type_name != nullptr);

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

    auto* sym = ctx.parser_.lookup_unique_symbol(node->identifier->symbol_index);
    assert(sym != nullptr);

    if(!node->init_value) {
        if(sym->type.flags & TYPE_IS_ARRAY && array_has_inferred_sizes(sym->type)) {
            ctx.raise_error("Arrays with inferred sizes (e.g. '[]') must be assigned when created.", node->pos);
            return std::nullopt;
        }

        return sym->type;
    }


    //
    // Array declaration
    //

    if((*node->init_value)->type == NODE_BRACED_EXPRESSION && sym->type.flags & TYPE_IS_ARRAY) {
        const auto array_t = get_bracedexpr_as_array_t(dynamic_cast<ast_braced_expression*>(*node->init_value), ctx);
        if(!array_t) {
            ctx.raise_error("Could not deduce type of righthand expression.", node->pos);
            return std::nullopt;
        }

        if(!are_array_types_equivalent(sym->type, *array_t)) {
            ctx.raise_error(fmt("Array of type {} is not equivalent to {}.",
                typedata_to_str_msg(*array_t),
                typedata_to_str_msg(sym->type)),
                (*node->init_value)->pos
            );

            return std::nullopt;
        }

        assert(array_t->array_lengths.size() == sym->type.array_lengths.size());
        for(size_t i = 0; i < array_t->array_lengths.size(); i++) {
            sym->type.array_lengths[i] = array_t->array_lengths[i];
        }

        return sym->type;
    }


    //
    // Struct declaration
    //

    if((*node->init_value)->type == NODE_BRACED_EXPRESSION && sym->type.kind == TYPE_KIND_STRUCT) {
        check_structassign_bracedexpr(sym->type, dynamic_cast<ast_braced_expression*>(*node->init_value), ctx);
        return sym->type;
    }


    //
    // Declaration of primitive or other
    //

    const auto init_t = visit_node(*node->init_value, ctx);
    if(!init_t) {
        ctx.raise_error("Righthand expression does not have a type.", node->pos);
        return std::nullopt;
    }

    if(!is_type_coercion_permissible(sym->type, *init_t)) {
        ctx.raise_error(fmt("Cannot assign variable \"{}\" of type {} to {}.",
            sym->name,
            typedata_to_str_msg(sym->type),
            typedata_to_str_msg(*init_t)),
            node->pos
        );

        return std::nullopt;
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

    const std::string target_t_str = typedata_to_str_msg(*target_t);
    const std::string cast_t_str   = typedata_to_str_msg(cast_t);

    if(!is_type_cast_permissible(*target_t, cast_t)) {
        ctx.raise_error(fmt("Cannot cast type {} to {}.", target_t_str, cast_t_str), node->pos);
        return std::nullopt;
    }

    return cast_t;
}


std::optional<type_data>
visit_procdecl(const ast_procdecl* node, checker_context& ctx) {

    assert(node != nullptr);

    for(const auto& child : node->body) {
        if(NODE_NEEDS_VISITING(child->type)) {
            visit_node(child, ctx);
        }
    }

    return std::nullopt;
}


std::optional<type_data>
visit_call(const ast_call* node, checker_context& ctx) {

    assert(node != nullptr);

    auto target_t = visit_node(node->target, ctx);
    if(!target_t) {
        ctx.raise_error("Unable to deduce type of call target", node->pos);
        return std::nullopt;
    }

    if(target_t->kind != TYPE_KIND_PROCEDURE) {
        ctx.raise_error("Attempt to call non-callable type.", node->pos);
        return std::nullopt;
    }


    const auto     proc_t_str  = typedata_to_str_msg(*target_t);
    const uint32_t called_with = node->arguments.size();
    const uint32_t receives    = target_t->parameters == nullptr ? 0 : target_t->parameters->size();

    if(called_with != receives) {
        ctx.raise_error(fmt("Attempting to call procedure of type {} with {} arguments, but it takes {}.",
            proc_t_str,
            called_with,
            receives),
            node->pos
        );

         if(target_t->return_type == nullptr) {
             return std::nullopt;
         }

        target_t->return_type->flags |= TYPE_IS_CONSTANT | TYPE_RVALUE;
        return *target_t->return_type;
    }

    if(!called_with && !receives) {
        target_t->return_type->flags |= TYPE_IS_CONSTANT | TYPE_RVALUE;
        return *target_t->return_type;
    }


    assert(target_t->parameters != nullptr);
    for(size_t i = 0; i < node->arguments.size(); i++) {
        if(const auto arg_t = visit_node(node->arguments[i], ctx)) {
            if(!is_type_coercion_permissible((*target_t->parameters)[i], *arg_t)) {

                const std::string param_t_str = typedata_to_str_msg((*target_t->parameters)[i]);
                const std::string arg_t_str   = typedata_to_str_msg(*arg_t);

                ctx.raise_error(fmt("Cannot convert argument {} of type {} to expected parameter type {}.",
                    i + 1, arg_t_str, param_t_str), node->arguments[i]->pos);
            }
        } else {
            ctx.raise_error("Cannot deduce type of argument {} in this call.", i + 1);
        }
    }

    if(target_t->return_type == nullptr) {
        return std::nullopt;
    }

    target_t->return_type->flags |= TYPE_IS_CONSTANT | TYPE_RVALUE;
    return target_t;
}


std::optional<type_data>
visit_ret(const ast_ret* node, checker_context& ctx) {

    assert(node != nullptr);

    const ast_node*     itr  = node;
    const ast_procdecl* proc = nullptr;
    const symbol*       sym  = nullptr;

    do {
        assert(itr->parent.has_value());
        itr = *itr->parent;
    } while(itr->type != NODE_PROCDECL);


    proc = dynamic_cast<const ast_procdecl*>(itr);
    assert(proc != nullptr);
    sym  = ctx.parser_.lookup_unique_symbol(proc->identifier->symbol_index);
    assert(sym != nullptr);


    size_t count = 0;
    if(node->value.has_value())          ++count;
    if(sym->type.return_type != nullptr) ++count;

    if(count == 0) {
        return std::nullopt;
    }

    if(count != 2) {
        ctx.raise_error(fmt("Invalid return statement: does not match return type for procedure \"{}\".", sym->name), node->pos);
        return std::nullopt;
    }


    const auto ret_t = visit_node(*node->value, ctx);
    if(!ret_t) {
        ctx.raise_error("Could not deduce type of righthand expression.", node->pos);
        return std::nullopt;
    }

    if(!is_type_coercion_permissible(*sym->type.return_type, *ret_t)) {
        ctx.raise_error(fmt("Cannot coerce type {} to procedure return type {} (compiling procedure \"{}\").",
            typedata_to_str_msg(*ret_t),
            typedata_to_str_msg(*sym->type.return_type),
            sym->name),
            node->pos
        );
    }

    return *ret_t;
}


std::optional<type_data>
visit_member_access(const ast_member_access* node, checker_context& ctx) {

    assert(node != nullptr);
    const auto target_t = visit_node(node->target, ctx);

    if(!target_t) {
        ctx.raise_error("Attempting to access non-existant type as a struct.", node->pos);
        return std::nullopt;
    }

    if(target_t->kind != TYPE_KIND_STRUCT || target_t->flags & TYPE_IS_ARRAY || target_t->pointer_depth > 1) {
        ctx.raise_error(fmt("Cannot perform member access on type {}.", typedata_to_str_msg(*target_t)), node->pos);
        return std::nullopt;
    }

    const auto* base_type_name = std::get_if<std::string>(&target_t->name);
    assert(base_type_name != nullptr);

    if(const auto member_type = get_struct_member_type_data(node->path, *base_type_name, ctx.parser_)) {
        return *member_type;
    }

    ctx.raise_error(fmt("Struct member \"{}\" within type \"{}\" does not exist.", node->path, typedata_to_str_msg(*target_t)), node->pos);
    return std::nullopt;
}


std::optional<type_data>
visit_node(ast_node* node, checker_context& ctx) {

    assert(node != nullptr);
    assert(node->type != NODE_NONE);

    switch(node->type) {
        case NODE_VARDECL:           return visit_vardecl(dynamic_cast<ast_vardecl*>(node), ctx);
        case NODE_PROCDECL:          return visit_procdecl(dynamic_cast<ast_procdecl*>(node), ctx);
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
        case NODE_BLOCK:             break;
        case NODE_CALL:              return visit_call(dynamic_cast<ast_call*>(node), ctx);
        case NODE_RET:               return visit_ret(dynamic_cast<ast_ret*>(node), ctx);
        case NODE_DEFER:
        case NODE_SIZEOF:
        case NODE_SUBSCRIPT:
        case NODE_NAMESPACEDECL:
        case NODE_MEMBER_ACCESS:     return visit_member_access(dynamic_cast<ast_member_access*>(node), ctx);
        case NODE_BRACED_EXPRESSION: return std::nullopt;

        default: break;
    }

    panic("visit_node: invalid AST node passed.");
}



