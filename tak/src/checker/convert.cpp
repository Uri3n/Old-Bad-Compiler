//
// Created by Diago on 2024-07-30.
//

#include <checker.hpp>


template<typename T>
concept is_arithmetic = std::integral<T> || std::floating_point<T>;

template<is_arithmetic T, is_arithmetic J>
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
    type.kind  = TYPE_KIND_PRIMITIVE;
    type.name  = PRIMITIVE_U64;

    try {
        actual = std::stoll(node->value);
    } catch(...) {
        panic("convert_int_lit_to_concrete: exception converting string -> i64.");
    }

    if(is_t_within_range<uint8_t>(actual))       type.name = PRIMITIVE_U8;
    else if(is_t_within_range<uint16_t>(actual)) type.name = PRIMITIVE_U16;
    else if(is_t_within_range<uint32_t>(actual)) type.name = PRIMITIVE_U32;

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
    type.kind  = TYPE_KIND_PRIMITIVE;
    type.name  = PRIMITIVE_F64;

    try {
        actual = std::stod(node->value);
    } catch(...) {
        panic("convert_float_lit_to_concrete: exception converting string -> double.");
    }

    if(is_t_within_range<float>(actual))
        type.name = PRIMITIVE_F32;

    return type;
}


std::optional<tak::TypeData>
tak::get_bracedexpr_as_array_t(const AstBracedExpression* node, CheckerContext& ctx, const bool only_literals) {

    assert(node != nullptr);
    if(node->members.empty()) {
        return std::nullopt;
    }

    auto contained_t = [&]() -> std::optional<TypeData> {
        if(node->members[0]->type == NODE_BRACED_EXPRESSION) {
            return get_bracedexpr_as_array_t(dynamic_cast<AstBracedExpression*>(node->members[0]), ctx, only_literals);
        } else {
            return evaluate(node->members[0], ctx);
        }
    }();


    if(!contained_t || TypeData::is_invalid_in_inferred_context(*contained_t)) {
        return std::nullopt;
    }

    if(only_literals && !(contained_t->flags & TYPE_ARRAY) && node->members[0]->type != NODE_SINGLETON_LITERAL) {
        ctx.errs_.raise_error("Only literals are permitted in this context.", node->members[0]);
        return std::nullopt;
    }

    if(contained_t->flags & TYPE_ARRAY && node->members[0]->type != NODE_BRACED_EXPRESSION) {
        ctx.errs_.raise_error("Array type is invalid in this context.", node->members[0]);
        return std::nullopt;
    }


    for(size_t i = 1; i < node->members.size(); i++) {
        if(node->members[i]->type == NODE_BRACED_EXPRESSION) {
            const auto subarray_t = get_bracedexpr_as_array_t(dynamic_cast<AstBracedExpression*>(node->members[i]), ctx, only_literals);
            if(!subarray_t) {
                return std::nullopt;
            }

            if(!(contained_t->flags & TYPE_ARRAY) || !TypeData::are_arrays_equivalent(*contained_t, *subarray_t)) {
                return std::nullopt;
            }
        } else {
            const auto element_t = evaluate(node->members[i], ctx);
            if(!element_t || !TypeData::is_coercion_permissible(*contained_t, *element_t)) {
                return std::nullopt;
            }

            if(element_t->flags & TYPE_ARRAY) {
                ctx.errs_.raise_error("Array type is invalid in this context.", node->members[i]);
                return std::nullopt;
            }

            if(only_literals && node->members[i]->type != NODE_SINGLETON_LITERAL) {
                ctx.errs_.raise_error("Only literals are permitted in this context.", node->members[i]);
                return std::nullopt;
            }
        }
    }

    contained_t->flags |= TYPE_ARRAY;
    contained_t->array_lengths.insert(contained_t->array_lengths.begin(), node->members.size());
    return contained_t;
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
tak::assign_bracedexpr_to_struct(const TypeData& type, const AstBracedExpression* expr, CheckerContext& ctx, const bool only_literals) {

    assert(expr != nullptr);
    assert(type.kind == TYPE_KIND_STRUCT);

    if(type.flags & TYPE_RVALUE) {
        ctx.errs_.raise_error(fmt("Cannot assign this braced expression to lefthand type {}.", TypeData::to_string(type)), expr);
        return;
    }

    const std::string        name    = std::get<std::string>(type.name);
    std::vector<MemberData>* members = ctx.tbl_.lookup_type_members(name);

    //
    // Validate that sizes are correct
    //

    if(members->size() != expr->members.size()) {
        ctx.errs_.raise_error(fmt("Number of elements within braced expression ({}) does not match the struct type {} ({} members).",
            expr->members.size(),
            TypeData::to_string(type),
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

            const auto array_t = get_bracedexpr_as_array_t(dynamic_cast<AstBracedExpression*>(expr->members[i]), ctx, only_literals);
            if(!array_t) {
                ctx.errs_.raise_error(fmt("Element {} in braced expression is invalid.", i + 1), expr);
                continue;
            }

            if(!TypeData::are_arrays_equivalent(members->at(i).type, *array_t)) {
                ctx.errs_.raise_error(fmt("Element {} in braced expression: array of type {} is not equivalent to {}.",
                    i + 1, TypeData::to_string(*array_t), TypeData::to_string(members->at(i).type)), expr);
            }
            continue;
        }

        if(members->at(i).type.kind == TYPE_KIND_STRUCT && expr->members[i]->type == NODE_BRACED_EXPRESSION) {
            assign_bracedexpr_to_struct((*members)[i].type, dynamic_cast<AstBracedExpression*>(expr->members[i]), ctx, only_literals);
            continue;
        }

        const auto element_t = evaluate(expr->members[i], ctx);
        if(!element_t) {
            ctx.errs_.raise_error(fmt("Could not deduce type of element {} in braced expression.", i + 1), expr);
            continue;
        }

        if(only_literals && expr->members[i]->type != NODE_SINGLETON_LITERAL) {
            ctx.errs_.raise_error("Only literals are permitted in this context.", expr->members[i]);
            continue;
        }

        if(!TypeData::is_coercion_permissible(members->at(i).type, *element_t)) {
            ctx.errs_.raise_error(fmt("Cannot coerce element {} of braced expression to type {} ({} was given).",
                i + 1,
                TypeData::to_string(members->at(i).type),
                TypeData::to_string(*element_t)),
                expr->members[i]
            );
        }
    }
}