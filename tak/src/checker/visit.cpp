//
// Created by Diago on 2024-07-22.
//

#include <checker.hpp>


template<typename T>
concept ast_node_has_children = requires(T a)
{
    { a->children } -> std::same_as<std::vector<tak::AstNode*>&>;
};

template<typename T> requires ast_node_has_children<T>
static auto visit_node_children(T node, tak::CheckerContext& ctx) -> std::optional<tak::TypeData> {

    assert(node != nullptr);
    for(auto* child : node->children) {
        if(NODE_NEEDS_VISITING(child->type)) {
            tak::visit_node(child, ctx);
        }
    }

    return std::nullopt;
}


std::optional<tak::TypeData>
tak::visit_binexpr(AstBinexpr* node, CheckerContext& ctx) {

    assert(node != nullptr);

    auto  left_t = visit_node(node->left_op, ctx);
    auto right_t = visit_node(node->right_op, ctx);

    if(left_t.has_value()
        && left_t->kind         == TYPE_KIND_STRUCT
        && node->right_op->type == NODE_BRACED_EXPRESSION
        && node->_operator      == TOKEN_VALUE_ASSIGNMENT
        && can_operator_be_applied_to(TOKEN_VALUE_ASSIGNMENT, *left_t)
    ) {
        assign_bracedexpr_to_struct(*left_t, dynamic_cast<AstBracedExpression*>(node->right_op), ctx);
        left_t->flags |=  TYPE_RVALUE;
        return *left_t;
    }

    if(!left_t || !right_t) {
        ctx.raise_error("Unable to deduce type of one or more operands.", node->pos);
        return std::nullopt;
    }


    const auto op_as_str    = token_to_string(node->_operator);
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
        TypeData constbool;
        constbool.name   = VAR_BOOLEAN;
        constbool.kind   = TYPE_KIND_VARIABLE;
        constbool.flags  = TYPE_CONSTANT | TYPE_RVALUE;

        return constbool;
    }

    left_t->flags |= TYPE_RVALUE;
    return left_t;
}


std::optional<tak::TypeData>
tak::visit_unaryexpr(AstUnaryexpr* node, CheckerContext& ctx) {

    assert(node != nullptr);
    assert(node->operand != nullptr);

    auto operand_t = visit_node(node->operand, ctx);
    if(!operand_t) {
        return std::nullopt;
    }

    const auto op_as_str   = token_to_string(node->_operator);
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

        TypeData constbool;
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

        ctx.raise_error(fmt("Cannot get the address of type {}.", type_as_str), node->pos);
        return std::nullopt;
    }

    panic("visit_unaryexpr: no suitable operator found.");
}


std::optional<tak::TypeData>
tak::visit_identifier(const AstIdentifier* node, CheckerContext& ctx) {

    assert(node != nullptr);
    const auto* sym = ctx.parser_.lookup_unique_symbol(node->symbol_index);
    assert(sym != nullptr);
    if(sym->type.flags & TYPE_INFERRED || sym->type.flags & TYPE_UNINITIALIZED) {
        ctx.raise_error(fmt("Referencing uninitialized or invalid symbol \"{}\".", sym->name), node->pos);
        return std::nullopt;
    }

    return sym->type;
}


std::optional<tak::TypeData>
tak::visit_singleton_literal(AstSingletonLiteral* node, CheckerContext& ctx) {

    assert(node != nullptr);
    TypeData data;

    switch(node->literal_type) {
        case TOKEN_STRING_LITERAL:
            data.name          = VAR_I8;
            data.pointer_depth = 1;
            data.flags        |= TYPE_POINTER;
            break;

        case TOKEN_KW_NULLPTR:
            data.name          = VAR_VOID;
            data.pointer_depth = 1;
            data.flags        |= TYPE_POINTER | TYPE_NON_CONCRETE;
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
initialize_symbol(tak::Symbol* sym) {
    if(sym->type.flags & tak::TYPE_UNINITIALIZED) {
        sym->type.flags &= ~tak::TYPE_UNINITIALIZED;
    }
}


std::optional<tak::TypeData>
tak::checker_handle_arraydecl(Symbol* sym, const AstVardecl* decl, CheckerContext& ctx) {

    assert(sym != nullptr && decl != nullptr);
    assert(decl->init_value);

    const auto array_t = get_bracedexpr_as_array_t(dynamic_cast<AstBracedExpression*>(*decl->init_value), ctx);
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


std::optional<tak::TypeData>
tak::checker_handle_inferred_decl(Symbol* sym, const AstVardecl* decl, CheckerContext& ctx) {

    assert(sym != nullptr && decl != nullptr);
    assert(sym->type.flags & TYPE_INFERRED);
    assert(decl->init_value.has_value());

    std::optional<tak::TypeData> assigned_t;


    if((*decl->init_value)->type == NODE_BRACED_EXPRESSION) {
        assigned_t = get_bracedexpr_as_array_t(dynamic_cast<AstBracedExpression*>(*decl->init_value), ctx);
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


std::optional<tak::TypeData>
tak::visit_vardecl(const AstVardecl* node, CheckerContext& ctx) {

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
        assign_bracedexpr_to_struct(sym->type, dynamic_cast<AstBracedExpression*>(*node->init_value), ctx);
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


std::optional<tak::TypeData>
tak::visit_cast(const AstCast* node, CheckerContext& ctx) {

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


std::optional<tak::TypeData>
tak::visit_procdecl(const AstProcdecl* node, CheckerContext& ctx) {

    assert(node != nullptr);

    for(const auto& child : node->body) {
        if(NODE_NEEDS_VISITING(child->type)) {
            visit_node(child, ctx);
        }
    }

    return std::nullopt;
}


static bool
verify_method_call(const tak::TypeData& method_t, tak::AstCall* node, tak::CheckerContext& ctx) {

    assert(node != nullptr);
    assert(node->target != nullptr);
    assert(method_t.sym_ref != INVALID_SYMBOL_INDEX);

    auto*          maccess     = dynamic_cast<tak::AstMemberAccess*>(node->target);
    const uint32_t called_with = node->arguments.size();
    const uint32_t receives    = method_t.parameters == nullptr ? 0 : method_t.parameters->size();

    if(maccess == nullptr || called_with == receives) {
        ctx.raise_error(tak::fmt("Calling method with {} arguments, but it takes {}.", called_with, !receives ? receives : receives - 1), node->pos);
        return false;
    }


    const auto  struct_t = visit_node(maccess->target, ctx);
    const auto* symbol   = ctx.parser_.lookup_unique_symbol(method_t.sym_ref);
    auto*       ident    = new tak::AstIdentifier();

    assert(struct_t.has_value());
    assert(struct_t->kind == tak::TYPE_KIND_STRUCT);
    assert(struct_t->pointer_depth < 2);
    assert(struct_t->array_lengths.empty());
    assert(symbol != nullptr);


    if(!(struct_t->flags & tak::TYPE_POINTER)) {
        auto* unaryexpr      = new tak::AstUnaryexpr();               // so we can address the struct during codegen
        unaryexpr->_operator = tak::TOKEN_BITWISE_AND;                // address-of operator
        unaryexpr->operand   = maccess->target;                       // change call target to just the proc
        unaryexpr->pos       = maccess->pos;
        node->arguments.insert(node->arguments.begin(), unaryexpr);   // add the struct as a call argument.
    } else {
        node->arguments.insert(node->arguments.begin(), maccess->target);
    }


    node->target        = ident;
    ident->symbol_index = symbol->symbol_index;
    ident->pos          = symbol->src_pos;
    ident->parent       = node;
    maccess->target     = nullptr;
    delete maccess;


    if(receives != node->arguments.size()) {
        ctx.raise_error(tak::fmt("Calling method \"{}\" with {} arguments, but it takes {}.",
            symbol->name, node->arguments.size(), receives), node->pos);
        return false;
    }

    assert(symbol->type.parameters != nullptr);
    for(size_t i = 0; i < node->arguments.size(); i++) {
        const auto arg_t = visit_node(node->arguments[i], ctx);
        if(!arg_t) {
            ctx.raise_error(tak::fmt("Cannot deduce type of argument {} in this call.", i + 1), node->pos);
            continue;
        }

        if(!is_type_coercion_permissible((*symbol->type.parameters)[i], *arg_t)) {
            const std::string param_t_str = typedata_to_str_msg((*symbol->type.parameters)[i]);
            const std::string arg_t_str   = typedata_to_str_msg(*arg_t);

            ctx.raise_error(tak::fmt("Cannot convert argument {} of type {} to expected parameter type {}.",
                i + 1, arg_t_str, param_t_str), node->arguments[i]->pos);
        }
    }

    return true;
}


std::optional<tak::TypeData>
tak::visit_call(AstCall* node, CheckerContext& ctx) {

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

    if(target_t->flags & TYPE_PROC_METHOD && node->target->type == NODE_MEMBER_ACCESS) {
        if(verify_method_call(*target_t, node, ctx) && target_t->return_type != nullptr) {
            if(!is_returntype_lvalue_eligible(*target_t->return_type)) {
                return to_rvalue(*target_t->return_type);
            }

            return *target_t->return_type;
        }

        return std::nullopt;
    }


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

        if(!is_returntype_lvalue_eligible(*target_t->return_type)) {
            return to_rvalue(*target_t->return_type);
        }

        return *target_t->return_type;
    }


    if(!called_with && !receives) {
        if(target_t->return_type == nullptr) {
            return std::nullopt;
        }

        if(!is_returntype_lvalue_eligible(*target_t->return_type)) {
            return to_rvalue(*target_t->return_type);
        }

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
            ctx.raise_error(fmt("Cannot deduce type of argument {} in this call.", i + 1), node->pos);
        }
    }


    if(target_t->return_type == nullptr) {
        return std::nullopt;
    }

    if(!is_returntype_lvalue_eligible(*target_t->return_type)) {
        return to_rvalue(*target_t->return_type);
    }

    return *target_t->return_type;
}


std::optional<tak::TypeData>
tak::visit_ret(const AstRet* node, CheckerContext& ctx) {

    assert(node != nullptr);

    const AstNode*     itr  = node;
    const AstProcdecl* proc = nullptr;
    const Symbol*       sym  = nullptr;

    do {
        assert(itr->parent.has_value());
        itr = *itr->parent;
    } while(itr->type != NODE_PROCDECL);


    proc = dynamic_cast<const AstProcdecl*>(itr);
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


std::optional<tak::TypeData>
tak::visit_member_access(const AstMemberAccess* node, CheckerContext& ctx) {

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

    ctx.raise_error(fmt("Cannot access \"{}\" within type \"{}\".", node->path, typedata_to_str_msg(*target_t)), node->pos);
    return std::nullopt;
}


std::optional<tak::TypeData>
tak::visit_defer_if(const AstDeferIf* node, CheckerContext& ctx) {

    assert(node != nullptr);

    const auto condition_t = visit_node(node->condition, ctx);
    const auto call_t      = visit_node(node->call, ctx);

    if(!condition_t) {
        ctx.raise_error("defer_if condition does not produce a type.", node->condition->pos);
        return std::nullopt;
    }

    if(!is_type_lop_eligible(*condition_t)) {
        ctx.raise_error(fmt("Type {} cannot be used as a logical expression.", typedata_to_str_msg(*condition_t)), node->condition->pos);
    }

    return call_t;
}


std::optional<tak::TypeData>
tak::visit_defer(const AstDefer* node,  CheckerContext& ctx) {

    assert(node != nullptr);
    return visit_node(node->call, ctx); // Not much to do here. We just need to typecheck the wrapped call node
}


std::optional<tak::TypeData>
tak::visit_sizeof(const AstSizeof* node, CheckerContext& ctx) {

    assert(node != nullptr);
    if(const auto* is_child_node = std::get_if<AstNode*>(&node->target)) {
        if(!visit_node(*is_child_node, ctx)) {
            ctx.raise_error("Expression does not evaluate to a type.", (*is_child_node)->pos);
            return std::nullopt;
        }
    }

    TypeData const_int;
    const_int.kind   = TYPE_KIND_VARIABLE;
    const_int.name   = VAR_U8;
    const_int.flags |= TYPE_RVALUE | TYPE_CONSTANT | TYPE_NON_CONCRETE;

    return const_int;
}


std::optional<tak::TypeData>
tak::visit_branch(const AstBranch* node, CheckerContext& ctx) {

    assert(node != nullptr);
    for(const AstIf* _if : node->conditions) {
        const auto condition_t = visit_node(_if->condition, ctx);
        if(!condition_t) {
            ctx.raise_error("Invalid branch condition: contained expression does not produce a type.", _if->pos);
            continue;
        }

        const std::string cond_str = typedata_to_str_msg(*condition_t);
        if(!is_type_lop_eligible(*condition_t)) {
            ctx.raise_error(fmt("Type {} cannot be used as a logical expression.", cond_str), _if->condition->pos);
        }

        for(AstNode* branch_child : _if->body) {
            if(NODE_NEEDS_VISITING(branch_child->type)) visit_node(branch_child, ctx);
        }
    }

    if(node->_else) {
        for(AstNode* branch_child : (*node->_else)->body) {
            if(NODE_NEEDS_VISITING(branch_child->type)) visit_node(branch_child, ctx);
        }
    }

    return std::nullopt;
}


std::optional<tak::TypeData>
tak::visit_for(const AstFor* node, CheckerContext& ctx) {

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


std::optional<tak::TypeData>
tak::visit_switch(const AstSwitch* node, CheckerContext& ctx) {

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


    for(AstCase* _case : node->cases) {
        const auto case_t = visit_node(_case->value, ctx);
        if(!case_t) {
            ctx.raise_error("Case value does not produce a type.", _case->pos);
            continue;
        }

        if(!is_type_coercion_permissible(*target_t, *case_t)) {
            ctx.raise_error(fmt("Cannot coerce type of case value ({}) to {}.", typedata_to_str_msg(*case_t), target_t_str), _case->pos);
        }

        for(AstNode* child : _case->body) {
            if(NODE_NEEDS_VISITING(child->type)) visit_node(child, ctx);
        }
    }

    for(AstNode* child : node->_default->body) {
        if(NODE_NEEDS_VISITING(child->type)) visit_node(child, ctx);
    }

    return std::nullopt;
}


std::optional<tak::TypeData>
tak::visit_while(AstNode* node, CheckerContext& ctx) {

    assert(node != nullptr);
    assert(node->type == NODE_WHILE || node->type == NODE_DOWHILE);


    const std::vector<AstNode*>* branch_body = nullptr;
    AstNode* condition                       = nullptr;

    if(const auto* _while = dynamic_cast<AstWhile*>(node)) { // Could also just use templates but... eh.
        condition   = _while->condition;
        branch_body = &_while->body;
    }
    else if(const auto* _dowhile = dynamic_cast<AstDoWhile*>(node)) {
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

    for(AstNode* child : *branch_body) {
        if(NODE_NEEDS_VISITING(child->type)) visit_node(child, ctx);
    }

    return std::nullopt;
}


std::optional<tak::TypeData>
tak::visit_subscript(const AstSubscript* node, CheckerContext& ctx) {

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


std::optional<tak::TypeData>
tak::visit_node(AstNode* node, CheckerContext& ctx) {

    assert(node != nullptr);
    assert(node->type != NODE_NONE);

    switch(node->type) {
        case NODE_NAMESPACEDECL:     return visit_node_children(dynamic_cast<AstNamespaceDecl*>(node), ctx);
        case NODE_COMPOSEDECL:       return visit_node_children(dynamic_cast<AstComposeDecl*>(node), ctx);
        case NODE_BLOCK:             return visit_node_children(dynamic_cast<AstBlock*>(node), ctx);
        case NODE_VARDECL:           return visit_vardecl(dynamic_cast<AstVardecl*>(node), ctx);
        case NODE_PROCDECL:          return visit_procdecl(dynamic_cast<AstProcdecl*>(node), ctx);
        case NODE_BINEXPR:           return visit_binexpr(dynamic_cast<AstBinexpr*>(node), ctx);
        case NODE_UNARYEXPR:         return visit_unaryexpr(dynamic_cast<AstUnaryexpr*>(node), ctx);
        case NODE_SINGLETON_LITERAL: return visit_singleton_literal(dynamic_cast<AstSingletonLiteral*>(node), ctx);
        case NODE_IDENT:             return visit_identifier(dynamic_cast<AstIdentifier*>(node), ctx);
        case NODE_CAST:              return visit_cast(dynamic_cast<AstCast*>(node), ctx);
        case NODE_BRANCH:            return visit_branch(dynamic_cast<AstBranch*>(node), ctx);
        case NODE_FOR:               return visit_for(dynamic_cast<AstFor*>(node), ctx);
        case NODE_SWITCH:            return visit_switch(dynamic_cast<AstSwitch*>(node), ctx);
        case NODE_CALL:              return visit_call(dynamic_cast<AstCall*>(node), ctx);
        case NODE_RET:               return visit_ret(dynamic_cast<AstRet*>(node), ctx);
        case NODE_DEFER:             return visit_defer(dynamic_cast<AstDefer*>(node), ctx);
        case NODE_DEFER_IF:          return visit_defer_if(dynamic_cast<AstDeferIf*>(node), ctx);
        case NODE_SIZEOF:            return visit_sizeof(dynamic_cast<AstSizeof*>(node), ctx);
        case NODE_SUBSCRIPT:         return visit_subscript(dynamic_cast<AstSubscript*>(node), ctx);
        case NODE_MEMBER_ACCESS:     return visit_member_access(dynamic_cast<AstMemberAccess*>(node), ctx);
        case NODE_WHILE:             return visit_while(node, ctx);
        case NODE_DOWHILE:           return visit_while(node, ctx);
        case NODE_BRACED_EXPRESSION: return std::nullopt;

        default: break;
    }

    panic("visit_node: non-visitable node passed.");
}



