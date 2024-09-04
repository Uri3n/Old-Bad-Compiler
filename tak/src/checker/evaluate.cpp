//
// Created by Diago on 2024-07-22.
//

#include <checker.hpp>


template<typename T> requires tak::node_has_children<T>
static auto evaluate_children(T node, tak::CheckerContext& ctx) -> std::optional<tak::TypeData> {
    assert(node != nullptr);
    for(auto* child : node->children) {
        if(NODE_NEEDS_EVALUATING(child->type)) tak::evaluate(child, ctx);
    }

    return std::nullopt;
}


std::optional<tak::TypeData>
tak::evaluate_procdecl(AstProcdecl* node, CheckerContext& ctx) {
    assert(node != nullptr);
    Symbol* sym = ctx.tbl_.lookup_unique_symbol(node->identifier->symbol_index);

    const bool is_foreign   = sym->flags & ENTITY_FOREIGN || sym->flags & ENTITY_FOREIGN_C;
    const bool is_foreign_c = sym->flags & ENTITY_FOREIGN_C;
    const bool is_intern    = sym->flags & ENTITY_INTERNAL;

    if(is_foreign && is_intern) {
        ctx.errs_.raise_error("Cannot create a procedure that is marked as both extern and intern.", node);
    }

    if(sym->flags & ENTITY_FOREIGN && !node->children.empty()) {
        ctx.errs_.raise_error("Procedures marked as foreign should not have bodies defined here.", node);
    }

    if(is_foreign_c && sym->flags & ENTITY_FOREIGN) {
        ctx.errs_.raise_warning("both extern \"C\" and regular extern specified, assuming \"C\".", node);
        sym->flags &= ~ENTITY_FOREIGN;
    }

    return evaluate_children(node, ctx);
}


static bool
binexpr_ptr_arith_chk(const tak::TypeData& left, const tak::TypeData& right, const tak::token_t _operator) {
    const auto* prim_left   = std::get_if<tak::primitive_t>(&left.name);
    const auto* prim_right  = std::get_if<tak::primitive_t>(&right.name);

    const bool is_valid_ptr = left.flags & tak::TYPE_POINTER
        && !(left.flags & tak::TYPE_ARRAY)
        && !(prim_left != nullptr
        && *prim_left == tak::PRIMITIVE_VOID
        && left.pointer_depth < 2);

    const bool is_valid_operand = !(right.flags & tak::TYPE_POINTER)
        && !(right.flags & tak::TYPE_ARRAY)
        && (prim_right != nullptr
        && PRIMITIVE_IS_INTEGRAL(*prim_right));

    return is_valid_ptr && is_valid_operand && TOKEN_OP_IS_VALID_PTR_ARITH(_operator);
}


std::optional<tak::TypeData>
tak::evaluate_binexpr(AstBinexpr* node, CheckerContext& ctx) {
    assert(node != nullptr);
    auto  left_t = evaluate(node->left_op, ctx);
    auto right_t = evaluate(node->right_op, ctx);

    if(left_t.has_value()
        && left_t->kind         == TYPE_KIND_STRUCT
        && node->right_op->type == NODE_BRACED_EXPRESSION
        && node->_operator      == TOKEN_VALUE_ASSIGNMENT
        && !(left_t->flags      &  TYPE_POINTER)
        && TypeData::can_operator_be_applied_to(TOKEN_VALUE_ASSIGNMENT, *left_t)
    ) {
        assign_bracedexpr_to_struct(*left_t, dynamic_cast<AstBracedExpression*>(node->right_op), ctx);
        left_t->flags |= TYPE_RVALUE;
        return *left_t;
    }

    if(!left_t || !right_t) {
        ctx.errs_.raise_error("Unable to deduce type of one or more operands.", node);
        return std::nullopt;
    }


    const auto op_as_str    = Token::to_string(node->_operator);
    const auto left_as_str  = left_t->to_string();
    const auto right_as_str = right_t->to_string();

    if(node->_operator == TOKEN_CONDITIONAL_OR || node->_operator == TOKEN_CONDITIONAL_AND) {
        if(!TypeData::can_operator_be_applied_to(node->_operator, *left_t)) {
            ctx.errs_.raise_error(fmt("Logical operator '{}' cannot be applied to lefthand type {}.", op_as_str, left_as_str), node);
        }
        if(!TypeData::can_operator_be_applied_to(node->_operator, *right_t)) {
            ctx.errs_.raise_error(fmt("Logical operator '{}' cannot be applied to righthand type {}.", op_as_str, right_as_str), node);
        }
    } else {
        if(!TypeData::can_operator_be_applied_to(node->_operator, *left_t)) {
            ctx.errs_.raise_error(fmt("Operator '{}' cannot be applied to lefthand type {}.", op_as_str, left_as_str), node);
        }
        if(!binexpr_ptr_arith_chk(*left_t, *right_t, node->_operator) && !TypeData::is_coercion_permissible(*left_t, *right_t)) {
            ctx.errs_.raise_error(fmt("Cannot coerce type of righthand expression ({}) to {}", right_as_str, left_as_str), node);
        }
    }


    if(TOKEN_OP_IS_LOGICAL(node->_operator)) {
        return TypeData::get_const_bool();
    }

    left_t->flags |= TYPE_RVALUE;
    return left_t;
}


std::optional<tak::TypeData>
tak::evaluate_unaryexpr(AstUnaryexpr* node, CheckerContext& ctx) {
    assert(node != nullptr);
    assert(node->operand != nullptr);

    auto operand_t = evaluate(node->operand, ctx);
    if(!operand_t) {
        return std::nullopt;
    }

    const auto op_as_str   = Token::to_string(node->_operator);
    const auto type_as_str = operand_t->to_string();

    if(node->_operator == TOKEN_SUB || node->_operator == TOKEN_PLUS) {
        if(!operand_t->is_primitive() || operand_t->kind != TYPE_KIND_PRIMITIVE) {
            ctx.errs_.raise_error(fmt("Cannot apply unary operator {} to type {}.", op_as_str, type_as_str), node);
            return std::nullopt;
        }

        if(node->_operator == TOKEN_SUB) {
            if(!operand_t->flip_sign()) {
                ctx.errs_.raise_error(fmt("Cannot apply unary minus to type {}.", op_as_str, type_as_str), node);
                return std::nullopt;
            }
        }

        operand_t->flags |= TYPE_RVALUE;
        return operand_t;
    }

    if(node->_operator == TOKEN_BITWISE_NOT) {
        if(!operand_t->is_bwop_eligible()) {
            ctx.errs_.raise_error(fmt("Cannot apply bitwise operator ~ to type {}", type_as_str), node);
            return std::nullopt;
        }

        operand_t->flags |= TYPE_RVALUE;
        return operand_t;
    }

    if(node->_operator == TOKEN_INCREMENT || node->_operator == TOKEN_DECREMENT) {
        if(!TypeData::can_operator_be_applied_to(node->_operator, *operand_t)) {
            ctx.errs_.raise_error(fmt("Cannot apply operator {} to type {}", op_as_str, type_as_str), node);
            return std::nullopt;
        }

        operand_t->flags |= TYPE_RVALUE;
        return operand_t;
    }

    if(node->_operator == TOKEN_CONDITIONAL_NOT) {
        if(!operand_t->is_lop_eligible()) {
            ctx.errs_.raise_error(fmt("Cannot apply logical operator ! to type {}", type_as_str), node);
            return std::nullopt;
        }

        return TypeData::get_const_bool();
    }

    if(node->_operator == TOKEN_BITWISE_XOR_OR_PTR) {
        if(const auto deref_t = TypeData::get_contained(*operand_t)) {
            return deref_t;
        }

        ctx.errs_.raise_error(fmt("Cannot dereference type {}.", type_as_str), node);
        return std::nullopt;
    }

    if(node->_operator == TOKEN_BITWISE_AND) {
        if(const auto addressed_t = TypeData::get_pointer_to(*operand_t)) {
            return addressed_t;
        }

        ctx.errs_.raise_error(fmt("Cannot get the address of type {}.", type_as_str), node);
        return std::nullopt;
    }

    panic("evaluate_unaryexpr: no suitable operator found.");
}


std::optional<tak::TypeData>
tak::evaluate_identifier(const AstIdentifier* node, CheckerContext& ctx) {
    assert(node != nullptr);
    const auto* sym = ctx.tbl_.lookup_unique_symbol(node->symbol_index);
    assert(sym != nullptr);

    if(sym->flags & ENTITY_GENBASE) {
        ctx.errs_.raise_error(fmt("Cannot access a generic procedure without type arguments."), node);
        return std::nullopt;
    }

    if(sym->type.flags & TYPE_INFERRED || sym->flags & ENTITY_PLACEHOLDER) {
        ctx.errs_.raise_error(fmt("Referencing uninitialized or invalid symbol \"{}\".", sym->name), node);
        return std::nullopt;
    }

    return sym->type;
}


std::optional<tak::TypeData>
tak::evaluate_singleton_literal(AstSingletonLiteral* node, CheckerContext& _) {
    assert(node != nullptr);
    TypeData data;

    switch(node->literal_type) {
        case TOKEN_STRING_LITERAL:
            data = TypeData::get_const_string();
            break;
        case TOKEN_KW_NULLPTR:
            data = TypeData::get_const_voidptr();
            break;
        case TOKEN_FLOAT_LITERAL:
            data = convert_float_lit_to_type(node);
            break;
        case TOKEN_INTEGER_LITERAL:
            data = convert_int_lit_to_type(node);
            break;
        case TOKEN_CHARACTER_LITERAL:
            data = TypeData::get_const_char();
            break;
        case TOKEN_BOOLEAN_LITERAL:
            data = TypeData::get_const_bool();
            break;

        default: panic("evaluate_singleton_literal: default case reached.");
    }

    data.flags |= TYPE_CONSTANT | TYPE_RVALUE | TYPE_NON_CONCRETE;
    data.kind   = TYPE_KIND_PRIMITIVE;
    return { data };
}


std::optional<tak::TypeData>
tak::evaluate_arraydecl(Symbol* sym, const AstVardecl* decl, CheckerContext& ctx) {
    assert(sym != nullptr && decl != nullptr);
    assert(decl->init_value);

    if((*decl->init_value)->type != NODE_BRACED_EXPRESSION) {
        ctx.errs_.raise_error("Expected braced initializer.", decl);
        return std::nullopt;
    }

    const auto array_t = get_bracedexpr_as_array_t(dynamic_cast<AstBracedExpression*>(*decl->init_value), ctx, sym->flags & ENTITY_GLOBAL);
    if(!array_t) {
        ctx.errs_.raise_error("Could not deduce type of righthand expression.", decl);
        return std::nullopt;
    }

    if(!TypeData::are_arrays_equivalent(sym->type, *array_t)) {
        ctx.errs_.raise_error(fmt("Array of type {} is not equivalent to {}.",
            array_t->to_string(), sym->type.to_string()), *decl->init_value
        );

        return std::nullopt;
    }

    assert(array_t->array_lengths.size() == sym->type.array_lengths.size());
    for(size_t i = 0; i < array_t->array_lengths.size(); i++) {
        sym->type.array_lengths[i] = array_t->array_lengths[i];
    }

    return sym->type;
}


std::optional<tak::TypeData>
tak::evaluate_inferred_decl(Symbol* sym, const AstVardecl* decl, CheckerContext& ctx) {
    assert(sym != nullptr && decl != nullptr);
    assert(sym->type.flags & TYPE_INFERRED);
    assert(decl->init_value.has_value());

    std::optional<TypeData> assigned_t;
    const bool only_lits = sym->flags & ENTITY_GLOBAL;


    //
    // If braced expression is being assigned check if we can create an array type.
    //

    if((*decl->init_value)->type == NODE_BRACED_EXPRESSION) {
        assigned_t = get_bracedexpr_as_array_t(dynamic_cast<AstBracedExpression*>(*decl->init_value), ctx, only_lits);
    } else {
        assigned_t = evaluate(*decl->init_value, ctx);
    }

    if(!assigned_t) {
        ctx.errs_.raise_error(fmt("Expression assigned to \"{}\" does not have a type.", sym->name), decl);
        return std::nullopt;
    }

    if(only_lits && !(assigned_t->flags & TYPE_ARRAY) && (*decl->init_value)->type != NODE_SINGLETON_LITERAL) {
        ctx.errs_.raise_error("Only literals are permitted in this context.", *decl->init_value);
        return std::nullopt;
    }

    if(assigned_t->is_invalid_in_inferred_context() || (assigned_t->flags & TYPE_ARRAY && (*decl->init_value)->type != NODE_BRACED_EXPRESSION)) {
        ctx.errs_.raise_error(fmt("Cannot assign type {} in an inferred context.", assigned_t->to_string()), *decl->init_value);
        return std::nullopt;
    }


    //
    // Automatically promote type if the assigned type is a non-concrete float
    // or integer literal.
    //

    const primitive_t* is_prim  = std::get_if<primitive_t>(&assigned_t->name);

    if(is_prim != nullptr
        && assigned_t->flags   & TYPE_NON_CONCRETE
        && !(assigned_t->flags & TYPE_POINTER)
        && (PRIMITIVE_IS_INTEGRAL(*is_prim) || PRIMITIVE_IS_FLOAT(*is_prim))
    ){
        if(PRIMITIVE_IS_FLOAT(*is_prim)) {
            assigned_t->name = PRIMITIVE_F64;
        } else if(primitive_t_size_bytes(*is_prim) <= primitive_t_size_bytes(PRIMITIVE_I32)) {
            assigned_t->name = PRIMITIVE_I32;
        }
    }


    //
    // Under almost all scenarios remove constness from assigned type, because that should be determined
    // by the inferred decl not the other type.
    // The only exception is when a const i8^ (string lit) is being assigned to the inferred declaration.
    //

    sym->type.flags   &= ~TYPE_INFERRED;
    assigned_t->flags &= ~TYPE_CONSTANT;
    assigned_t->flags &= ~TYPE_NON_CONCRETE;
    assigned_t->flags &= ~TYPE_RVALUE;

    const uint64_t tmp_flags = sym->type.flags;
    sym->type        = *assigned_t;
    sym->type.flags |= tmp_flags;

    return sym->type;
}


std::optional<tak::TypeData>
tak::evaluate_vardecl(const AstVardecl* node, CheckerContext& ctx) {
    assert(node != nullptr);
    assert(node->identifier != nullptr);

    auto* sym = ctx.tbl_.lookup_unique_symbol(node->identifier->symbol_index);

    if(sym->flags & ENTITY_INTERNAL && sym->flags & ENTITY_FOREIGN) {
        ctx.errs_.raise_error("Variable is marked both as foreign and internal.", node);
    }
    if(sym->flags & ENTITY_FOREIGN && node->init_value.has_value()) {
        ctx.errs_.raise_error("Declarations of foreign variables cannot be initialized.", node);
    }
    if(!node->init_value) {
        assert(!(sym->type.flags & TYPE_INFERRED));
        if(sym->type.flags & TYPE_ARRAY && sym->type.array_has_inferred_sizes()) {
            ctx.errs_.raise_error("Arrays with inferred sizes (e.g. '[]') must be assigned when created.", node);
            return std::nullopt;
        }

        return sym->type;
    }


    if(sym->type.flags & TYPE_INFERRED) {
        return evaluate_inferred_decl(sym, node, ctx);
    }
    if(sym->type.flags & TYPE_ARRAY) {
        return evaluate_arraydecl(sym, node, ctx);
    }
    if((*node->init_value)->type == NODE_BRACED_EXPRESSION && sym->type.kind == TYPE_KIND_STRUCT && !(sym->type.flags & TYPE_POINTER)) {
        assign_bracedexpr_to_struct(sym->type, dynamic_cast<AstBracedExpression*>(*node->init_value), ctx, sym->flags & ENTITY_GLOBAL);
        return sym->type;
    }


    const auto init_t = evaluate(*node->init_value, ctx);
    if(!init_t) {
        ctx.errs_.raise_error("Righthand expression does not have a type.", node);
        return std::nullopt;
    }
    if(sym->flags & ENTITY_GLOBAL && (*node->init_value)->type != NODE_SINGLETON_LITERAL) {
        ctx.errs_.raise_error("Globals must be initialized using literals.", node);
    }
    if(!TypeData::is_coercion_permissible(sym->type, *init_t)) {
        ctx.errs_.raise_error(fmt("Cannot assign variable \"{}\" of type {} to {}.",
            sym->name,
            sym->type.to_string(),
            init_t->to_string()),
            node
        );

        return std::nullopt;
    }

    return sym->type;
}


std::optional<tak::TypeData>
tak::evaluate_cast(const AstCast* node, CheckerContext& ctx) {
    assert(node != nullptr);
    const auto  target_t = evaluate(node->target, ctx);
    const auto& cast_t   = node->type;

    if(!target_t) {
        return std::nullopt;
    }

    const std::string target_t_str = target_t->to_string();
    const std::string cast_t_str   = cast_t.to_string();

    if(!TypeData::is_cast_permissible(*target_t, cast_t)) {
        ctx.errs_.raise_error(fmt("Cannot cast type {} to {}.", target_t_str, cast_t_str), node);
        return std::nullopt;
    }

    return cast_t;
}


static std::optional<tak::TypeData>
get_call_return_type(const tak::TypeData& called) {
    if(called.return_type == nullptr) {
        return std::nullopt;
    }
    if(!called.return_type->is_returntype_lvalue_eligible()) {
        return tak::TypeData::to_rvalue(*called.return_type);
    }

    return *called.return_type;
}

static bool
is_t_invalid_as_procarg(const tak::TypeData& type) {
    return type.flags & tak::TYPE_ARRAY
        || (type.kind == tak::TYPE_KIND_PROCEDURE
        && !(type.flags & tak::TYPE_POINTER));
}


static std::optional<tak::TypeData>
evaluate_variadic_call(const tak::AstCall* node, const tak::TypeData& called, tak::CheckerContext& ctx) {
    assert(node != nullptr);
    assert(called.kind == tak::TYPE_KIND_PROCEDURE);
    assert(called.flags & tak::TYPE_PROC_VARARGS);

    const auto     proc_t_str  = called.to_string();
    const uint32_t called_with = node->arguments.size();
    const uint32_t receives    = called.parameters == nullptr ? 0 : called.parameters->size();

    if(receives > called_with) {
        ctx.errs_.raise_error(tak::fmt("Invalid number of arguments passed (requires at least {}).", receives), node);
        return get_call_return_type(called);
    }

    for(size_t i = 0; i < node->arguments.size(); i++) {
        const auto arg_t = evaluate(node->arguments[i], ctx);
        if(!arg_t.has_value()) {
            ctx.errs_.raise_error(tak::fmt("Cannot deduce type of argument {} in this call.", i + 1), node->arguments[i]);
            continue;
        }

        if(is_t_invalid_as_procarg(*arg_t)) {
            ctx.errs_.raise_error(tak::fmt("Type {} cannot be used as a procedure argument.", arg_t->to_string()), node->arguments[i]);
            continue;
        }

        if(called.parameters != nullptr
            && i < called.parameters->size()
            && !tak::TypeData::is_coercion_permissible(called.parameters->at(i), *arg_t)
        ) {
            ctx.errs_.raise_error(tak::fmt("Cannot convert argument {} of type {} to expected parameter type {}.",
                i + 1, arg_t->to_string(), called.parameters->at(i).to_string()), node->arguments[i]);
        }
    }

    return get_call_return_type(called);
}


std::optional<tak::TypeData>
tak::evaluate_call(AstCall* node, CheckerContext& ctx) {
    assert(node != nullptr);

    auto target_t = evaluate(node->target, ctx);
    if(!target_t) {
        ctx.errs_.raise_error("Unable to deduce type of call target", node);
        return std::nullopt;
    }

    if(target_t->kind != TYPE_KIND_PROCEDURE || target_t->pointer_depth > 1) {
        ctx.errs_.raise_error("Attempt to call non-callable type.", node);
        return std::nullopt;
    }

    if(target_t->flags & TYPE_PROC_VARARGS) {
        return evaluate_variadic_call(node, *target_t, ctx);
    }


    const auto     proc_t_str  = target_t->to_string();
    const uint32_t called_with = node->arguments.size();
    const uint32_t receives    = target_t->parameters == nullptr ? 0 : target_t->parameters->size();

    if(called_with != receives) {
        ctx.errs_.raise_error(fmt("Attempting to call procedure of type {} with {} arguments, but it takes {}.",
            proc_t_str,
            called_with,
            receives),
            node
        );

        return get_call_return_type(*target_t);
    }

    if(!called_with && !receives) {
        return get_call_return_type(*target_t);
    }

    assert(target_t->parameters != nullptr);
    for(size_t i = 0; i < node->arguments.size(); i++) {
        const auto arg_t = evaluate(node->arguments[i], ctx);
        if(!arg_t) {
            ctx.errs_.raise_error(fmt("Cannot deduce type of argument {} in this call.", i + 1), node->arguments[i]);
            continue;
        }

        if(is_t_invalid_as_procarg(*arg_t)) {
            ctx.errs_.raise_error(fmt("Type {} cannot be used as a procedure argument.", arg_t->to_string()), node->arguments[i]);
            continue;
        }

        if(!TypeData::is_coercion_permissible(target_t->parameters->at(i), *arg_t)) {
            const std::string param_t_str = target_t->parameters->at(i).to_string();
            const std::string arg_t_str   = arg_t->to_string();

            ctx.errs_.raise_error(fmt("Cannot convert argument {} of type {} to expected parameter type {}.",
                i + 1, arg_t_str, param_t_str), node->arguments[i]);
        }
    }

    return get_call_return_type(*target_t);
}


std::optional<tak::TypeData>
tak::evaluate_ret(const AstRet* node, CheckerContext& ctx) {
    assert(node != nullptr);

    const AstNode* itr = node;
    do {
        assert(itr->parent.has_value());
        itr = *itr->parent;
    } while(itr->type != NODE_PROCDECL);


    const auto* proc = dynamic_cast<const AstProcdecl*>(itr);
    assert(proc != nullptr);
    const auto* sym  = ctx.tbl_.lookup_unique_symbol(proc->identifier->symbol_index);
    assert(sym != nullptr);


    size_t count = 0;
    if(node->value.has_value())          ++count;
    if(sym->type.return_type != nullptr) ++count;

    if(count == 0) {
        return std::nullopt;
    }

    if(count != 2) {
        ctx.errs_.raise_error(fmt("Invalid return statement: does not match return type for procedure \"{}\".", sym->name), node);
        return std::nullopt;
    }


    const auto ret_t = evaluate(*node->value, ctx);
    if(!ret_t) {
        ctx.errs_.raise_error("Could not deduce type of righthand expression.", node);
        return std::nullopt;
    }

    if(!TypeData::is_coercion_permissible(*sym->type.return_type, *ret_t)) {
        ctx.errs_.raise_error(fmt("Cannot coerce type {} to procedure return type {} (compiling procedure \"{}\").",
            ret_t->to_string(),
            sym->type.return_type->to_string(),
            sym->name),
            node
        );
    }

    return ret_t;
}


std::optional<tak::TypeData>
tak::evaluate_member_access(const AstMemberAccess* node, CheckerContext& ctx) {
    assert(node != nullptr);

    const auto target_t = evaluate(node->target, ctx);
    if(!target_t) {
        ctx.errs_.raise_error("Attempting to access non-existant type as a struct.", node);
        return std::nullopt;
    }

    if(target_t->kind != TYPE_KIND_STRUCT || target_t->flags & TYPE_ARRAY || target_t->pointer_depth > 1) {
        ctx.errs_.raise_error(fmt("Cannot perform member access on type {}.", target_t->to_string()), node);
        return std::nullopt;
    }

    const auto* base_type_name = std::get_if<std::string>(&target_t->name);
    assert(base_type_name != nullptr);

    if(auto member_type = get_struct_member_type_data(node->path, *base_type_name, ctx.tbl_)) {
        if(target_t->flags & TYPE_CONSTANT) {
            member_type->flags |= TYPE_CONSTANT;
        }
        return *member_type;
    }

    ctx.errs_.raise_error(fmt("Cannot access \"{}\" within type \"{}\".", node->path, target_t->to_string()), node);
    return std::nullopt;
}


std::optional<tak::TypeData>
tak::evaluate_defer_if(const AstDeferIf* node, CheckerContext& ctx) {
    assert(node != nullptr);

    const auto condition_t = evaluate(node->condition, ctx);
    const auto call_t      = evaluate(node->call, ctx);

    if(!condition_t) {
        ctx.errs_.raise_error("defer_if condition does not produce a type.", node->condition);
        return std::nullopt;
    }

    if(!condition_t->is_lop_eligible()) {
        ctx.errs_.raise_error(fmt("Type {} cannot be used as a logical expression.", condition_t->to_string()), node->condition);
    }

    return call_t;
}


std::optional<tak::TypeData>
tak::evaluate_defer(const AstDefer* node,  CheckerContext& ctx) {
    assert(node != nullptr);
    return evaluate(node->call, ctx); // Not much to do here. We just need to typecheck the wrapped call node
}


std::optional<tak::TypeData>
tak::evaluate_sizeof(const AstSizeof* node, CheckerContext& ctx) {
    assert(node != nullptr);

    if(const auto* is_child_node = std::get_if<AstNode*>(&node->target)) {
        const auto eval = evaluate(*is_child_node, ctx);
        if(!eval) {
            ctx.errs_.raise_error("Expression does not evaluate to a type.", *is_child_node);
            return std::nullopt;
        }

        if(eval->kind == TYPE_KIND_PROCEDURE && !(eval->flags & TYPE_POINTER)) {
            ctx.errs_.raise_error(fmt("Cannot get the size of type {}.", eval->to_string()), *is_child_node);
            return std::nullopt;
        }
    }

    return TypeData::get_const_int32();
}


std::optional<tak::TypeData>
tak::evaluate_branch(const AstBranch* node, CheckerContext& ctx) {
    assert(node != nullptr);

    const auto condition_t = evaluate(node->_if->condition, ctx);
    if(!condition_t) {
        ctx.errs_.raise_error("Invalid branch condition: contained expression does not produce a type.", node->_if);
    } else {
        const std::string cond_str = condition_t->to_string();
        if(!condition_t->is_lop_eligible()) {
            ctx.errs_.raise_error(fmt("Type {} cannot be used as a logical expression.", cond_str), node->_if);
        }
    }

    for(AstNode* branch_child : node->_if->body) {
        if(NODE_NEEDS_EVALUATING(branch_child->type)) evaluate(branch_child, ctx);
    }

    if(node->_else) {
        for(AstNode* branch_child : (*node->_else)->body) {
            if(NODE_NEEDS_EVALUATING(branch_child->type)) evaluate(branch_child, ctx);
        }
    }

    return std::nullopt;
}


std::optional<tak::TypeData>
tak::evaluate_for(const AstFor* node, CheckerContext& ctx) {
    assert(node != nullptr);

    if(node->init) {
        if(const auto init_t = evaluate(*node->init, ctx); !init_t) {
            ctx.errs_.raise_error("For-loop initialization clause does not produce a type.", *node->init);
        }
    }

    if(node->condition) {
        const auto condition_t = evaluate(*node->condition, ctx);
        if(!condition_t) {
            ctx.errs_.raise_error("For-loop condition does not produce a type.", *node->condition);
        } else if(!condition_t->is_lop_eligible()) {
            const std::string cond_t_str = condition_t->to_string();
            ctx.errs_.raise_error(fmt("Type {} cannot be used as a for-loop condition.", cond_t_str), *node->condition);
        }
    }

    if(node->update) {
        evaluate(*node->update, ctx);
    }

    for(AstNode* child : node->body) {
        if(NODE_NEEDS_EVALUATING(child->type)) {
            evaluate(child, ctx);
        }
    }

    return std::nullopt;
}


std::optional<tak::TypeData>
tak::evaluate_switch(const AstSwitch* node, CheckerContext& ctx) {
    assert(node != nullptr);
    assert(node->target != nullptr);

    auto target_t = evaluate(node->target, ctx);
    if(!target_t) {
        ctx.errs_.raise_error("Switch target does not produce a type.", node->target);
        return std::nullopt;
    }

    const std::string target_t_str = target_t->to_string();
    if(!target_t->is_bwop_eligible()) {
        ctx.errs_.raise_error(fmt("Type {} cannot be used as a switch target.", target_t_str), node->target);
        return std::nullopt;
    }


    for(AstCase* _case : node->cases) {
        const auto case_t = evaluate(_case->value, ctx);
        if(!case_t) {
            ctx.errs_.raise_error("Case value does not produce a type.", _case);
            continue;
        }
        if(!TypeData::is_coercion_permissible(*target_t, *case_t)) {
            ctx.errs_.raise_error(fmt("Cannot coerce type of case value ({}) to {}.", case_t->to_string(), target_t_str), _case);
        }
        for(AstNode* child : _case->body) {
            if(NODE_NEEDS_EVALUATING(child->type)) evaluate(child, ctx);
        }
    }

    for(AstNode* child : node->_default->body) {
        if(NODE_NEEDS_EVALUATING(child->type)) evaluate(child, ctx);
    }

    return std::nullopt;
}


std::optional<tak::TypeData>
tak::evaluate_while(AstNode* node, CheckerContext& ctx) {
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
        panic("evaluate_while: passed ast node is not a while-loop.");
    }


    assert(condition != nullptr && branch_body != nullptr);
    const auto condition_t = evaluate(condition, ctx);

    if(!condition_t) {
        ctx.errs_.raise_error("Loop condition does not produce a type.", condition);
    }

    else if(!condition_t->is_lop_eligible()) {
        ctx.errs_.raise_error(fmt("Type {} cannot be used as a condition for a while-loop.", condition_t->to_string()), condition);
    }

    for(AstNode* child : *branch_body) {
        if(NODE_NEEDS_EVALUATING(child->type)) evaluate(child, ctx);
    }

    return std::nullopt;
}


std::optional<tak::TypeData>
tak::evaluate_subscript(const AstSubscript* node, CheckerContext& ctx) {
    assert(node != nullptr);

    const auto value_t   = evaluate(node->value, ctx);
    const auto operand_t = evaluate(node->operand, ctx);

    if(!value_t)   ctx.errs_.raise_error("Value within subscript operator does not evaluate to a type.", node->value);
    if(!operand_t) ctx.errs_.raise_error("Subscript operand does not evaluate to a type.", node->operand);

    if(!value_t || !operand_t) {
        return std::nullopt;
    }

    const std::string value_t_str   = value_t->to_string();
    const std::string operand_t_str = operand_t->to_string();
    const auto        contained_t   = TypeData::get_contained(*operand_t);

    if(!value_t->is_bwop_eligible()) {
        ctx.errs_.raise_error(fmt("Type {} Cannot be used as a subscript value.", value_t_str), node->value);
    }

    if(!contained_t) {
        ctx.errs_.raise_error(fmt("Type {} cannot be subscripted into.", operand_t_str), node->operand);
        return std::nullopt;
    }

    return contained_t;
}


std::optional<tak::TypeData>
tak::evaluate_brk_or_cont(const AstNode* node, CheckerContext& ctx) {
    assert(node != nullptr);
    assert(node->type == NODE_CONT || node->type == NODE_BRK);

    const AstNode* itr = node;
    while(itr->type != NODE_FOR && itr->type != NODE_WHILE && itr->type != NODE_DOWHILE) {
        if(!itr->parent.has_value()) {
            ctx.errs_.raise_error("This statement must be within a loop.", node);
            break;
        }

        itr = *itr->parent;
    }

    return std::nullopt;
}


std::optional<tak::TypeData>
tak::evaluate(AstNode* node, CheckerContext& ctx) {
    assert(node != nullptr);
    assert(node->type != NODE_NONE);

    switch(node->type) {
        case NODE_NAMESPACEDECL:     return evaluate_children(dynamic_cast<AstNamespaceDecl*>(node), ctx);
        case NODE_BLOCK:             return evaluate_children(dynamic_cast<AstBlock*>(node), ctx);
        case NODE_PROCDECL:          return evaluate_procdecl(dynamic_cast<AstProcdecl*>(node), ctx);
        case NODE_VARDECL:           return evaluate_vardecl(dynamic_cast<AstVardecl*>(node), ctx);
        case NODE_BINEXPR:           return evaluate_binexpr(dynamic_cast<AstBinexpr*>(node), ctx);
        case NODE_UNARYEXPR:         return evaluate_unaryexpr(dynamic_cast<AstUnaryexpr*>(node), ctx);
        case NODE_SINGLETON_LITERAL: return evaluate_singleton_literal(dynamic_cast<AstSingletonLiteral*>(node), ctx);
        case NODE_IDENT:             return evaluate_identifier(dynamic_cast<AstIdentifier*>(node), ctx);
        case NODE_CAST:              return evaluate_cast(dynamic_cast<AstCast*>(node), ctx);
        case NODE_BRANCH:            return evaluate_branch(dynamic_cast<AstBranch*>(node), ctx);
        case NODE_FOR:               return evaluate_for(dynamic_cast<AstFor*>(node), ctx);
        case NODE_SWITCH:            return evaluate_switch(dynamic_cast<AstSwitch*>(node), ctx);
        case NODE_CALL:              return evaluate_call(dynamic_cast<AstCall*>(node), ctx);
        case NODE_RET:               return evaluate_ret(dynamic_cast<AstRet*>(node), ctx);
        case NODE_DEFER:             return evaluate_defer(dynamic_cast<AstDefer*>(node), ctx);
        case NODE_DEFER_IF:          return evaluate_defer_if(dynamic_cast<AstDeferIf*>(node), ctx);
        case NODE_SIZEOF:            return evaluate_sizeof(dynamic_cast<AstSizeof*>(node), ctx);
        case NODE_SUBSCRIPT:         return evaluate_subscript(dynamic_cast<AstSubscript*>(node), ctx);
        case NODE_MEMBER_ACCESS:     return evaluate_member_access(dynamic_cast<AstMemberAccess*>(node), ctx);
        case NODE_WHILE:             return evaluate_while(node, ctx);
        case NODE_DOWHILE:           return evaluate_while(node, ctx);
        case NODE_BRK:               return evaluate_brk_or_cont(node, ctx);
        case NODE_CONT:              return evaluate_brk_or_cont(node, ctx);
        case NODE_BRACED_EXPRESSION: return std::nullopt;

        default: break;
    }

    panic("evaluate: non-evaluateable node passed.");
}



