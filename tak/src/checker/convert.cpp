//
// Created by Diago on 2024-07-30.
//

#include <checker.hpp>


template<typename T>
concept arithmetic = std::integral<T> || std::floating_point<T>;

template<arithmetic T, arithmetic J>
static bool is_t_within_range(J val) {
    return val >= std::numeric_limits<T>::min() && val <= std::numeric_limits<T>::max();
}


tak::TypeData
tak::convert_int_lit_to_type(const AstSingletonLiteral* node) {

    assert(node != nullptr);
    assert(node->literal_type == TOKEN_INTEGER_LITERAL);
    assert(!node->value.empty());

    uint64_t actual = 0;
    TypeData type;

    type.flags = TYPE_CONSTANT | TYPE_NON_CONCRETE | TYPE_RVALUE;
    type.kind  = TYPE_KIND_VARIABLE;
    type.name  = VAR_U64;

    try {
        actual = std::stoll(node->value);
    } catch(...) {
        panic("convert_int_lit_to_concrete: exception converting string -> i64.");
    }

    if(is_t_within_range<uint8_t>(actual))       type.name = VAR_U8;
    else if(is_t_within_range<uint16_t>(actual)) type.name = VAR_U16;
    else if(is_t_within_range<uint32_t>(actual)) type.name = VAR_U32;

    return type;
}


tak::TypeData
tak::convert_float_lit_to_type(const AstSingletonLiteral* node) {

    assert(node != nullptr);
    assert(node->literal_type == TOKEN_FLOAT_LITERAL);
    assert(!node->value.empty());

    double    actual = 0.0;
    TypeData type;

    type.flags = TYPE_CONSTANT | TYPE_NON_CONCRETE | TYPE_RVALUE;
    type.kind  = TYPE_KIND_VARIABLE;
    type.name  = VAR_F64;

    try {
        actual = std::stod(node->value);
    } catch(...) {
        panic("convert_float_lit_to_concrete: exception converting string -> double.");
    }

    if(is_t_within_range<float>(actual))
        type.name = VAR_F32;

    return type;
}

tak::TypeData
tak::to_lvalue(const TypeData& type) {
    TypeData lval = type;
    lval.flags   &= ~TYPE_RVALUE;
    return lval;
}

tak::TypeData
tak::to_rvalue(const TypeData& type) {
    TypeData rval = type;
    rval.flags   |= TYPE_RVALUE;
    return rval;
}

bool
tak::type_promote_non_concrete(TypeData& left, const TypeData& right) {

    const auto* pleft_t  = std::get_if<var_t>(&left.name);
    const auto* pright_t = std::get_if<var_t>(&right.name);
    bool is_signed       = false;

    assert(left.flags & TYPE_NON_CONCRETE);
    assert(left.pointer_depth == 0    && right.pointer_depth == 0);
    assert(left.array_lengths.empty() && right.array_lengths.empty());
    assert(pleft_t != nullptr         && pright_t != nullptr);


    if(PRIMITIVE_IS_SIGNED(*pleft_t) || PRIMITIVE_IS_SIGNED(*pright_t)) {
        is_signed = true;
    }

    if(var_t_to_size_bytes(*pright_t) > var_t_to_size_bytes(*pleft_t)) {
        left = right;
    }


    if(is_signed) {
        switch(std::get<var_t>(left.name)) {
            case VAR_U8:   left.name = VAR_I8;  break;
            case VAR_U16:  left.name = VAR_I16; break;
            case VAR_U32:  left.name = VAR_I32; break;
            case VAR_U64:  left.name = VAR_I64; break;
            default: break;
        }
    }

    return true;
}


bool
tak::flip_sign(TypeData& type) {

    assert(type.pointer_depth == 0);
    assert(type.array_lengths.empty());
    assert(type.kind == TYPE_KIND_VARIABLE);

    const auto* ptype = std::get_if<var_t>(&type.name);
    assert(ptype != nullptr);

    switch(*ptype) {
        case VAR_U8:  type.name = VAR_I8;  break;
        case VAR_I8:  type.name = VAR_U8;  break;
        case VAR_U16: type.name = VAR_I16; break;
        case VAR_I16: type.name = VAR_U16; break;
        case VAR_U32: type.name = VAR_I32; break;
        case VAR_I32: type.name = VAR_U32; break;
        case VAR_U64: type.name = VAR_I64; break;
        case VAR_I64: type.name = VAR_U64; break;
        case VAR_BOOLEAN: return false;
        case VAR_VOID:    return false;
        default: break;
    }

    return true;
}


std::optional<tak::TypeData>
tak::get_bracedexpr_as_array_t(const AstBracedExpression* node, CheckerContext& ctx) {

    assert(node != nullptr);
    if(node->members.empty()) {
        return std::nullopt;
    }

    std::optional<TypeData> contained_t;
    if(node->members[0]->type == NODE_BRACED_EXPRESSION) {
        contained_t = get_bracedexpr_as_array_t(dynamic_cast<AstBracedExpression*>(node->members[0]), ctx);
    } else {
        contained_t = visit_node(node->members[0], ctx);
    }

    if(!contained_t || is_type_invalid_in_inferred_context(*contained_t)) {
        return std::nullopt;
    } if(contained_t->flags & TYPE_ARRAY && node->members[0]->type != NODE_BRACED_EXPRESSION) {
        ctx.errs_.raise_error("Array type is invalid in this context.", node->members[0]);
        return std::nullopt;
    }


    for(size_t i = 1; i < node->members.size(); i++) {
        if(node->members[i]->type == NODE_BRACED_EXPRESSION) {
            const auto subarray_t = get_bracedexpr_as_array_t(dynamic_cast<AstBracedExpression*>(node->members[i]), ctx);
            if(!subarray_t) {
                return std::nullopt;
            }

            if(!(contained_t->flags & TYPE_ARRAY) || !are_array_types_equivalent(*contained_t, *subarray_t)) {
                return std::nullopt;
            }
        } else {
            const auto element_t = visit_node(node->members[i], ctx);
            if(!element_t || !is_type_coercion_permissible(*contained_t, *element_t)) {
                return std::nullopt;
            } if(element_t->flags & TYPE_ARRAY) {
                ctx.errs_.raise_error("Array type is invalid in this context.", node->members[i]);
                return std::nullopt;
            }
        }
    }

    contained_t->flags |= TYPE_ARRAY;
    contained_t->array_lengths.insert(contained_t->array_lengths.begin(), node->members.size());
    return contained_t;
}


std::optional<tak::TypeData>
tak::get_dereferenced_type(const TypeData& type) {

    TypeData deref_t = type;

    if(deref_t.flags & TYPE_ARRAY) {
        assert(!deref_t.array_lengths.empty());
        deref_t.array_lengths.pop_back();

        if(deref_t.array_lengths.empty()) {
            deref_t.flags &= ~TYPE_ARRAY;
        }
    }
    else if(deref_t.flags & TYPE_POINTER) {
        assert(deref_t.pointer_depth > 0);
        --deref_t.pointer_depth;

        if(!deref_t.pointer_depth) {
            deref_t.flags &= ~TYPE_POINTER;
        }
    }
    else {
        return std::nullopt;
    }


    if(deref_t.kind == TYPE_KIND_PROCEDURE && !(deref_t.flags & TYPE_POINTER)) {
        return std::nullopt;
    }
    if(const auto* is_primitive = std::get_if<var_t>(&deref_t.name)) {
        if(*is_primitive == VAR_VOID && !(deref_t.flags & TYPE_POINTER)) {
            return std::nullopt;
        }
    }
    if(deref_t.flags & TYPE_RVALUE) {
        deref_t.flags &= ~TYPE_RVALUE;
    }

    return deref_t;
}


std::optional<tak::TypeData>
tak::get_addressed_type(const TypeData& type) {

    if(type.flags & TYPE_ARRAY || type.flags & TYPE_RVALUE) {
        return std::nullopt;
    }

    TypeData addressed_t = type;

    ++addressed_t.pointer_depth;
    addressed_t.flags |= TYPE_POINTER | TYPE_RVALUE;
    return addressed_t;
}


std::optional<tak::TypeData>
tak::get_struct_member_type_data(const std::string& member_path, const std::string& base_type_name, EntityTable& tbl) {

    const auto   member_chunks = split_string(member_path, '.');
    const auto*  type_data     = tbl.lookup_type(base_type_name);
    size_t       index         = 0;

    if(member_chunks.empty() || type_data == nullptr) {
        return std::nullopt;
    }


    std::function<std::optional<TypeData>(decltype(type_data))> recurse;
    recurse = [&](const decltype(type_data) type) -> std::optional<TypeData> {

        assert(type != nullptr);
        const auto mem_itr = std::find_if(type->members.begin(), type->members.end(), [&](const MemberData& member) {
            return member.name == member_chunks[index];
        });

        if(index + 1 >= member_chunks.size()) {
            if(mem_itr != type->members.end()) {
                return mem_itr->type;
            }
        }

        else if(mem_itr != type->members.end()) {
            if(const auto* struct_name = std::get_if<std::string>(&mem_itr->type.name)) {
                ++index;
                if(tbl.type_exists(*struct_name) && mem_itr->type.array_lengths.empty() && mem_itr->type.pointer_depth < 2) {
                    return recurse(tbl.lookup_type(*struct_name));
                }
            }
        }

        return std::nullopt;
    };

    return recurse(type_data);
}


void
tak::assign_bracedexpr_to_struct(const TypeData& type, const AstBracedExpression* expr, CheckerContext& ctx) {

    assert(expr != nullptr);
    assert(type.kind == TYPE_KIND_STRUCT);

    if(type.flags & TYPE_RVALUE) {
        ctx.errs_.raise_error(fmt("Cannot assign this braced expression to lefthand type {}.", typedata_to_str_msg(type)), expr);
        return;
    }

    const std::string* name = std::get_if<std::string>(&type.name);
    assert(name != nullptr);
    std::vector<MemberData>* members = ctx.tbl_.lookup_type_members(*name);
    assert(members != nullptr);


    //
    // Validate that sizes are correct
    //

    if(members->size() != expr->members.size()) {
        ctx.errs_.raise_error(fmt("Number of elements within braced expression ({}) does not match the struct type {} ({} members).",
            expr->members.size(),
            typedata_to_str_msg(type),
            members->size()),
            expr
        );

        return;
    }


    //
    // Match all members
    //

    for(size_t i = 0; i < members->size(); i++) {
        if(members->at(i).type.flags & TYPE_ARRAY)
        {
            if(expr->members[i]->type != NODE_BRACED_EXPRESSION) {
                ctx.errs_.raise_error(fmt("Element {} in braced expression is invalid.", i + 1), expr);
                continue;
            }

            const auto array_t = get_bracedexpr_as_array_t(dynamic_cast<AstBracedExpression*>(expr->members[i]), ctx);
            if(!array_t) {
                ctx.errs_.raise_error(fmt("Element {} in braced expression is invalid.", i + 1), expr);
                continue;
            }

            if(!are_array_types_equivalent(members->at(i).type, *array_t)) {
                ctx.errs_.raise_error(fmt("Element {} in braced expression: array of type {} is not equivalent to {}.",
                    i + 1, typedata_to_str_msg(*array_t), typedata_to_str_msg(members->at(i).type)), expr);
            }
            continue;
        }

        if(members->at(i).type.kind == TYPE_KIND_STRUCT && expr->members[i]->type == NODE_BRACED_EXPRESSION) {
            assign_bracedexpr_to_struct((*members)[i].type, dynamic_cast<AstBracedExpression*>(expr->members[i]), ctx);
            continue;
        }

        const auto element_t = visit_node(expr->members[i], ctx);
        if(!element_t) {
            ctx.errs_.raise_error(fmt("Could not deduce type of element {} in braced expression.", i + 1), expr);
            continue;
        }

        if(!is_type_coercion_permissible(members->at(i).type, *element_t)) {
            ctx.errs_.raise_error(fmt("Cannot coerce element {} of braced expression to type {} ({} was given).",
                i + 1,
                typedata_to_str_msg(members->at(i).type),
                typedata_to_str_msg(*element_t)),
                expr
            );
        }
    }
}