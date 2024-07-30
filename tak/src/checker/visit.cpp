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

    if(left_t.has_value()
        && left_t->kind         == TYPE_KIND_STRUCT
        && node->right_op->type == NODE_BRACED_EXPRESSION
        && node->_operator      == TOKEN_VALUE_ASSIGNMENT
        && can_operator_be_applied_to(TOKEN_VALUE_ASSIGNMENT, *left_t)
    ) {
        check_structassign_bracedexpr(*left_t, dynamic_cast<ast_braced_expression*>(node->right_op), ctx);
        left_t->flags |=  TYPE_RVALUE;
        return *left_t;
    }

    if(!left_t || !right_t) {
        ctx.raise_error("Unable to deduce type of one or more operands.", node->pos);
        return std::nullopt;
    }


    const auto op_as_str    = lexer_token_to_string(node->_operator);
    const auto left_as_str  = typedata_to_str_msg(*left_t);
    const auto right_as_str = typedata_to_str_msg(*right_t);

    if(node->_operator == TOKEN_CONDITIONAL_OR || node->_operator == TOKEN_CONDITIONAL_AND) {
        if(!can_operator_be_applied_to(node->_operator, *left_t)) {
            ctx.raise_error(fmt("Logical operator '{}' cannot be applied to lefthand type {}.", op_as_str, left_as_str), node->pos);
        }
        if(!can_operator_be_applied_to(node->_operator, *right_t)) {
            ctx.raise_error(fmt("Logical operator '{}' cannot be applied to righthand type {}.", op_as_str, right_as_str), node->pos);
        }
    } else {
        if(!can_operator_be_applied_to(node->_operator, *left_t)) {
            ctx.raise_error(fmt("Operator '{}' cannot be applied to lefthand type {}.", op_as_str, left_as_str), node->pos);
        }
        if(!is_type_coercion_permissible(*left_t, *right_t)) {
            ctx.raise_error(fmt("Cannot coerce type of righthand expression ({}) to {}", right_as_str, left_as_str), node->pos);
        }
    }


    if(TOKEN_OP_IS_LOGICAL(node->_operator)) {
        type_data constbool;
        constbool.name   = VAR_BOOLEAN;
        constbool.kind   = TYPE_KIND_VARIABLE;
        constbool.flags  = TYPE_CONSTANT | TYPE_RVALUE;

        return constbool;
    }

    left_t->flags |= TYPE_RVALUE;
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
        if(operand_t->flags & TYPE_POINTER || !operand_t->array_lengths.empty() || operand_t->kind != TYPE_KIND_VARIABLE) {
            ctx.raise_error(fmt("Cannot apply unary operator {} to type {}.", op_as_str, type_as_str), node->pos);
            return std::nullopt;
        }

        if(node->_operator == TOKEN_SUB) {
            if(!flip_sign(*operand_t)) {
                ctx.raise_error(fmt("Cannot apply unary minus to type {}.", op_as_str, type_as_str), node->pos);
                return std::nullopt;
            }
        }

        operand_t->flags |= TYPE_RVALUE;
        return operand_t;
    }

    if(node->_operator == TOKEN_BITWISE_NOT) {
        if(!is_type_bwop_eligible(*operand_t)) {
            ctx.raise_error(fmt("Cannot apply bitwise operator ~ to type {}", type_as_str), node->pos);
            return std::nullopt;
        }

        operand_t->flags |= TYPE_RVALUE;
        return operand_t;
    }

    if(node->_operator == TOKEN_INCREMENT || node->_operator == TOKEN_DECREMENT) {
        if(!can_operator_be_applied_to(node->_operator, *operand_t)) {
            ctx.raise_error(fmt("Cannot apply operator {} to type {}", op_as_str, type_as_str), node->pos);
            return std::nullopt;
        }

        operand_t->flags |= TYPE_RVALUE;
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
        constbool.flags  = TYPE_CONSTANT | TYPE_RVALUE;

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
visit_identifier(const ast_identifier* node, checker_context& ctx) {

    assert(node != nullptr);
    const auto* sym = ctx.parser_.lookup_unique_symbol(node->symbol_index);
    assert(sym != nullptr);
    if(sym->type.flags & TYPE_INFERRED || sym->type.flags & TYPE_UNINITIALIZED) {
        ctx.raise_error(fmt("Referencing uninitialized or invalid symbol \"{}\".", sym->name), node->pos);
        return std::nullopt;
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
            data.flags        |= TYPE_POINTER;
            break;

        case TOKEN_FLOAT_LITERAL:
            data = convert_float_lit_to_type(node);
            break;

        case TOKEN_INTEGER_LITERAL:
            data = convert_int_lit_to_type(node);
            break;

        case TOKEN_CHARACTER_LITERAL:
            data.name   = VAR_I8;
            data.flags |= TYPE_NON_CONCRETE;
            break;

        case TOKEN_BOOLEAN_LITERAL:
            data.name = VAR_BOOLEAN;
            break;

        default:
            panic("visit_singleton_literal: default case reached.");
    }

    data.flags |= TYPE_CONSTANT | TYPE_RVALUE;
    data.kind   = TYPE_KIND_VARIABLE;
    return { data };
}


static void
initialize_symbol(symbol* sym) {
    if(sym->type.flags & TYPE_UNINITIALIZED) {
        sym->type.flags &= ~TYPE_UNINITIALIZED;
    }
}


std::optional<type_data>
checker_handle_arraydecl(symbol* sym, const ast_vardecl* decl, checker_context& ctx) {

    assert(sym != nullptr && decl != nullptr);
    assert(decl->init_value);

    const auto array_t = get_bracedexpr_as_array_t(dynamic_cast<ast_braced_expression*>(*decl->init_value), ctx);
    if(!array_t) {
        ctx.raise_error("Could not deduce type of righthand expression.", decl->pos);
        return std::nullopt;
    }

    if(!are_array_types_equivalent(sym->type, *array_t)) {
        ctx.raise_error(fmt("Array of type {} is not equivalent to {}.",
            typedata_to_str_msg(*array_t),
            typedata_to_str_msg(sym->type)),
            (*decl->init_value)->pos
        );

        return std::nullopt;
    }

    assert(array_t->array_lengths.size() == sym->type.array_lengths.size());
    for(size_t i = 0; i < array_t->array_lengths.size(); i++) {
        sym->type.array_lengths[i] = array_t->array_lengths[i];
    }

    initialize_symbol(sym);
    return sym->type;
}


std::optional<type_data>
checker_handle_inferred_decl(symbol* sym, const ast_vardecl* decl, checker_context& ctx) {

    assert(sym != nullptr && decl != nullptr);
    assert(sym->type.flags & TYPE_INFERRED);
    assert(decl->init_value.has_value());

    std::optional<type_data> assigned_t;


    if((*decl->init_value)->type == NODE_BRACED_EXPRESSION) {
        assigned_t = get_bracedexpr_as_array_t(dynamic_cast<ast_braced_expression*>(*decl->init_value), ctx);
    } else {
        assigned_t = visit_node(*decl->init_value, ctx);
    }

    if(!assigned_t) {
        ctx.raise_error(fmt("Expression assigned to \"{}\" does not have a type.", sym->name), decl->pos);
        return std::nullopt;
    }

    if(is_type_invalid_in_inferred_context(*assigned_t) || (assigned_t->flags & TYPE_ARRAY && (*decl->init_value)->type != NODE_BRACED_EXPRESSION)) {
        ctx.raise_error(fmt("Cannot assign type {} in an inferred context.", typedata_to_str_msg(*assigned_t)), (*decl->init_value)->pos);
        return std::nullopt;
    }


    if(sym->type.flags & TYPE_GLOBAL)   assigned_t->flags |= TYPE_GLOBAL;
    if(sym->type.flags & TYPE_CONSTANT) assigned_t->flags |= TYPE_CONSTANT;
    if(sym->type.flags & TYPE_FOREIGN)  assigned_t->flags |= TYPE_FOREIGN;

    if(assigned_t->flags & TYPE_RVALUE)       assigned_t->flags &= ~TYPE_RVALUE;
    if(assigned_t->flags & TYPE_NON_CONCRETE) assigned_t->flags &= ~TYPE_NON_CONCRETE;

    sym->type = *assigned_t;
    return sym->type;
}


std::optional<type_data>
visit_vardecl(const ast_vardecl* node, checker_context& ctx) {

    assert(node != nullptr);
    assert(node->identifier != nullptr);

    auto* sym = ctx.parser_.lookup_unique_symbol(node->identifier->symbol_index);
    assert(sym != nullptr);

    if(!node->init_value) {
        initialize_symbol(sym);
        assert(!(sym->type.flags & TYPE_INFERRED));
        if(sym->type.flags & TYPE_ARRAY && array_has_inferred_sizes(sym->type)) {
            ctx.raise_error("Arrays with inferred sizes (e.g. '[]') must be assigned when created.", node->pos);
            return std::nullopt;
        }

        return sym->type;
    }


    if(sym->type.flags & TYPE_INFERRED) {
        return checker_handle_inferred_decl(sym, node, ctx);
    }

    if((*node->init_value)->type == NODE_BRACED_EXPRESSION && sym->type.flags & TYPE_ARRAY) {
        return checker_handle_arraydecl(sym, node, ctx);
    }

    if((*node->init_value)->type == NODE_BRACED_EXPRESSION && sym->type.kind == TYPE_KIND_STRUCT) {
        check_structassign_bracedexpr(sym->type, dynamic_cast<ast_braced_expression*>(*node->init_value), ctx);
        initialize_symbol(sym);
        return sym->type;
    }


    const auto init_t = visit_node(*node->init_value, ctx);
    if(!init_t) {
        ctx.raise_error("Righthand expression does not have a type.", node->pos);
        return std::nullopt;
    }

    initialize_symbol(sym);
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

        target_t->return_type->flags |= TYPE_RVALUE;
        return *target_t->return_type;
    }

    if(!called_with && !receives) {
        target_t->return_type->flags |= TYPE_RVALUE;
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

    target_t->return_type->flags |=TYPE_RVALUE;
    return *target_t->return_type;
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

    if(target_t->kind != TYPE_KIND_STRUCT || target_t->flags & TYPE_ARRAY || target_t->pointer_depth > 1) {
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
visit_defer(const ast_defer* node,  checker_context& ctx) {

    assert(node != nullptr);
    return visit_node(node->call, ctx); // Not much to do here. We just need to typecheck the wrapped call node
}


std::optional<type_data>
visit_sizeof(const ast_sizeof* node, checker_context& ctx) {

    assert(node != nullptr);
    if(const auto* is_child_node = std::get_if<ast_node*>(&node->target)) {
        if(!visit_node(*is_child_node, ctx)) {
            ctx.raise_error("Expression does not evaluate to a type.", (*is_child_node)->pos);
            return std::nullopt;
        }
    }

    type_data const_int;
    const_int.kind   = TYPE_KIND_VARIABLE;
    const_int.name   = VAR_U8;
    const_int.flags |= TYPE_RVALUE | TYPE_CONSTANT | TYPE_NON_CONCRETE;

    return const_int;
}


std::optional<type_data>
visit_namespacedecl(const ast_namespacedecl* node, checker_context& ctx) {

    assert(node != nullptr);
    for(ast_node* child : node->children) {
        if(NODE_NEEDS_VISITING(child->type)) {
            visit_node(child, ctx);
        }
    }

    return std::nullopt;
}


std::optional<type_data>
visit_block(const ast_block* node, checker_context& ctx) {

    assert(node != nullptr);
    for(ast_node* child : node->body) {
        if(NODE_NEEDS_VISITING(child->type)) {
            visit_node(child, ctx);
        }
    }

    return std::nullopt;
}


std::optional<type_data>
visit_branch(const ast_branch* node, checker_context& ctx) {

    assert(node != nullptr);
    for(const ast_if* _if : node->conditions) {
        const auto condition_t = visit_node(_if->condition, ctx);
        if(!condition_t) {
            ctx.raise_error("Invalid branch condition: contained expression does not produce a type.", _if->pos);
            continue;
        }

        const std::string cond_str = typedata_to_str_msg(*condition_t);
        if(!is_type_lop_eligible(*condition_t)) {
            ctx.raise_error(fmt("Type {} cannot be used as a logical expression.", cond_str), _if->condition->pos);
        }

        for(ast_node* branch_child : _if->body) {
            if(NODE_NEEDS_VISITING(branch_child->type)) visit_node(branch_child, ctx);
        }
    }

    if(node->_else) {
        for(ast_node* branch_child : (*node->_else)->body) {
            if(NODE_NEEDS_VISITING(branch_child->type)) visit_node(branch_child, ctx);
        }
    }

    return std::nullopt;
}


std::optional<type_data>
visit_for(const ast_for* node, checker_context& ctx) {

    assert(node != nullptr);
    if(node->init) {
        if(const auto init_t = visit_node(*node->init, ctx); !init_t) {
            ctx.raise_error("For-loop initialization clause does not produce a type.", (*node->init)->pos);
        }
    }

    if(node->condition) {
        const auto condition_t = visit_node(*node->condition, ctx);
        if(!condition_t) {
            ctx.raise_error("For-loop condition does not produce a type.",  (*node->condition)->pos);
        } else if(!is_type_lop_eligible(*condition_t)) {
            const std::string cond_t_str = typedata_to_str_msg(*condition_t);
            ctx.raise_error(fmt("Type {} cannot be used as a for-loop condition.", cond_t_str), (*node->condition)->pos);
        }
    }

    if(node->update) {
        visit_node(*node->update, ctx);
    }

    return std::nullopt;
}


std::optional<type_data>
visit_switch(const ast_switch* node, checker_context& ctx) {

    assert(node != nullptr && node->target != nullptr);
    auto target_t = visit_node(node->target, ctx);

    if(!target_t) {
        ctx.raise_error("Switch target does not produce a type.", node->target->pos);
        return std::nullopt;
    }

    const std::string target_t_str = typedata_to_str_msg(*target_t);

    if(!is_type_bwop_eligible(*target_t)) {
        ctx.raise_error(fmt("Type {} cannot be used as a switch target.", target_t_str), node->target->pos);
        return std::nullopt;
    }


    for(ast_case* _case : node->cases) {
        const auto case_t = visit_node(_case->value, ctx);
        if(!case_t) {
            ctx.raise_error("Case value does not produce a type.", _case->pos);
            continue;
        }

        if(!is_type_coercion_permissible(*target_t, *case_t)) {
            ctx.raise_error(fmt("Cannot coerce type of case value ({}) to {}.", typedata_to_str_msg(*case_t), target_t_str), _case->pos);
        }

        for(ast_node* child : _case->body) {
            if(NODE_NEEDS_VISITING(child->type)) visit_node(child, ctx);
        }
    }

    for(ast_node* child : node->_default->body) {
        if(NODE_NEEDS_VISITING(child->type)) visit_node(child, ctx);
    }

    return std::nullopt;
}


std::optional<type_data>
visit_while(ast_node* node, checker_context& ctx) {

    assert(node != nullptr);
    assert(node->type == NODE_WHILE || node->type == NODE_DOWHILE);


    const std::vector<ast_node*>* branch_body = nullptr;
    ast_node* condition                       = nullptr;

    if(const auto* _while = dynamic_cast<ast_while*>(node)) { // Could also just use templates but... eh.
        condition   = _while->condition;
        branch_body = &_while->body;
    }
    else if(const auto* _dowhile = dynamic_cast<ast_dowhile*>(node)) {
        condition   = _dowhile->condition;
        branch_body = &_dowhile->body;
    }
    else {
        panic("visit_while: passed ast node is not a while-loop.")
    }


    assert(condition != nullptr && branch_body != nullptr);
    const auto condition_t = visit_node(condition, ctx);

    if(!condition_t) {
        ctx.raise_error("Loop condition does not produce a type.", condition->pos);
    }

    else if(!is_type_lop_eligible(*condition_t)) {
        ctx.raise_error(fmt("Type {} cannot be used as a condition for a while-loop.", typedata_to_str_msg(*condition_t)), condition->pos);
    }

    for(ast_node* child : *branch_body) {
        if(NODE_NEEDS_VISITING(child->type)) visit_node(child, ctx);
    }

    return std::nullopt;
}


std::optional<type_data>
visit_subscript(const ast_subscript* node, checker_context& ctx) {

    assert(node != nullptr);

    const auto value_t   = visit_node(node->value, ctx);
    const auto operand_t = visit_node(node->operand, ctx);

    if(!value_t)   ctx.raise_error("Value within subscript operator does not evaluate to a type.", node->value->pos);
    if(!operand_t) ctx.raise_error("Subscript operand does not evaluate to a type.", node->operand->pos);

    if(!value_t || !operand_t) {
        return std::nullopt;
    }


    const std::string value_t_str   = typedata_to_str_msg(*value_t);
    const std::string operand_t_str = typedata_to_str_msg(*operand_t);
    const auto        deref_t       = get_dereferenced_type(*operand_t);

    if(!is_type_bwop_eligible(*value_t)) {
        ctx.raise_error(fmt("Type {} Cannot be used as a subscript value.", value_t_str), node->value->pos);
    }

    if(!deref_t) {
        ctx.raise_error(fmt("Type {} cannot be subscripted into.", operand_t_str), node->operand->pos);
        return std::nullopt;
    }

    return *deref_t;
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
        case NODE_BRANCH:            return visit_branch(dynamic_cast<ast_branch*>(node), ctx);
        case NODE_FOR:               return visit_for(dynamic_cast<ast_for*>(node), ctx);
        case NODE_SWITCH:            return visit_switch(dynamic_cast<ast_switch*>(node), ctx);
        case NODE_BLOCK:             return visit_block(dynamic_cast<ast_block*>(node), ctx);
        case NODE_CALL:              return visit_call(dynamic_cast<ast_call*>(node), ctx);
        case NODE_RET:               return visit_ret(dynamic_cast<ast_ret*>(node), ctx);
        case NODE_DEFER:             return visit_defer(dynamic_cast<ast_defer*>(node), ctx);
        case NODE_SIZEOF:            return visit_sizeof(dynamic_cast<ast_sizeof*>(node), ctx);
        case NODE_SUBSCRIPT:         return visit_subscript(dynamic_cast<ast_subscript*>(node), ctx);
        case NODE_NAMESPACEDECL:     return visit_namespacedecl(dynamic_cast<ast_namespacedecl*>(node), ctx);
        case NODE_MEMBER_ACCESS:     return visit_member_access(dynamic_cast<ast_member_access*>(node), ctx);
        case NODE_WHILE:             return visit_while(node, ctx);
        case NODE_DOWHILE:           return visit_while(node, ctx);
        case NODE_BRACED_EXPRESSION: return std::nullopt;

        default: break;
    }

    panic("visit_node: non-visitable node passed.");
}



