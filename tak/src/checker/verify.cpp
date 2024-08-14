//
// Created by Diago on 2024-07-30.
//

#include <checker.hpp>


bool
tak::types_are_identical(const TypeData& first, const TypeData& second) {

    if(first.kind != second.kind
        || first.pointer_depth != second.pointer_depth
        || first.array_lengths.size() != second.array_lengths.size()
    ) {
       return false;
    }

    for(size_t i = 0; i < first.array_lengths.size(); i++) {
        if(first.array_lengths[i] != second.array_lengths[i])
            return false;
    }


    //
    // Evaluate basic types first. Primitives and structures.
    //

    auto get_variant_t = [](auto&& arg) -> std::type_index { return typeid(arg); };
    if(std::visit(get_variant_t, first.name) != std::visit(get_variant_t, second.name)) {
        return false;
    }

    if(const auto* is_struct = std::get_if<std::string>(&first.name)) {
        assert(first.parameters == nullptr && second.parameters == nullptr);
        return *is_struct == std::get<std::string>(second.name);
    }

    if(const auto* is_var = std::get_if<var_t>(&first.name)) {
        assert(first.parameters == nullptr && second.parameters == nullptr);
        return *is_var == std::get<var_t>(second.name);
    }


    //
    // Otherwise symbol is a procedure. Validate that params and return type are identical.
    // A null return type or parameter list indicates the absenc of one. I.e. void return type or no params.
    //

    if(first.parameters != nullptr && second.parameters != nullptr) {
        if(first.parameters->size() != second.parameters->size()) {
            return false;
        }

        for (size_t i = 0 ; i < first.parameters->size(); i++) {
            if(!types_are_identical((*first.parameters)[i], (*second.parameters)[i]))
                return false;
        }

    } else if(!(first.parameters == nullptr && second.parameters == nullptr)) {
        return false;
    }


    if(first.return_type != nullptr && second.return_type != nullptr) {
        return types_are_identical(*first.return_type, *second.return_type);
    }

    return first.return_type == nullptr && second.return_type == nullptr;
}


bool
tak::is_type_cast_permissible(const TypeData& from, const TypeData& to) {

    if(types_are_identical(from, to)) {
        return true;
    }

    if(!is_type_cast_eligible(from) || !is_type_cast_eligible(to)) {
        return false;
    }


    const auto* primfrom_t = std::get_if<var_t>(&from.name);
    const auto* primto_t   = std::get_if<var_t>(&to.name);
    size_t      ptr_count  = 0;

    if(from.flags & TYPE_POINTER) ++ptr_count;
    if(to.flags & TYPE_POINTER)   ++ptr_count;


    if(ptr_count == 2 || ptr_count == 0) {
        return true;
    }

    if(from.flags & TYPE_POINTER && !(to.flags & TYPE_POINTER)) {
        return primto_t != nullptr && *primto_t == VAR_U64;
    }

    if(to.flags & TYPE_POINTER && !(from.flags & TYPE_POINTER)) {
        return primfrom_t != nullptr && *primfrom_t == VAR_U64;
    }

    return false;
}


bool
tak::is_type_coercion_permissible(TypeData& left, const TypeData& right) {

    if(types_are_identical(left, right)) {
        return true;
    }

    if(!is_type_cast_eligible(left) || !is_type_cast_eligible(right)) {
        return false;
    }


    size_t      ptr_count = 0;
    const auto* left_t    = std::get_if<var_t>(&left.name);
    const auto* right_t   = std::get_if<var_t>(&right.name);

    if(left.flags  & TYPE_POINTER) ++ptr_count;
    if(right.flags & TYPE_POINTER) ++ptr_count;

    if(ptr_count == 2) return (left_t != nullptr && *left_t == VAR_VOID && left.pointer_depth == 1) || right.flags & TYPE_NON_CONCRETE;
    if(ptr_count != 0) return false;


    if((left_t == nullptr || right_t == nullptr) || (PRIMITIVE_IS_FLOAT(*right_t) && !PRIMITIVE_IS_FLOAT(*left_t))) {
        return false;
    }

    if(left.flags & TYPE_NON_CONCRETE && !(left.flags & TYPE_POINTER) && !PRIMITIVE_IS_FLOAT(*left_t)) {
        return type_promote_non_concrete(left, right);
    }

    return var_t_to_size_bytes(*right_t) <= var_t_to_size_bytes(*left_t);
}


bool
tak::are_array_types_equivalent(const TypeData& first, const TypeData& second) {

    assert(first.flags & TYPE_ARRAY);
    assert(second.flags & TYPE_ARRAY);

    if(first.array_lengths.size() != second.array_lengths.size()) {
        return false;
    }

    for(size_t i = 0; i < first.array_lengths.size(); i++) {
        if(first.array_lengths[i]) if(first.array_lengths[i] != second.array_lengths[i]) return false;
    }


    TypeData first_contained;
    TypeData second_contained;

    first_contained.name           = first.name;
    first_contained.kind           = first.kind;
    first_contained.flags          = first.flags;
    first_contained.pointer_depth  = first.pointer_depth;
    first_contained.return_type    = first.return_type;
    first_contained.parameters     = first.parameters;

    second_contained.name          = second.name;
    second_contained.kind          = second.kind;
    second_contained.flags         = second.flags;
    second_contained.pointer_depth = second.pointer_depth;
    second_contained.return_type   = second.return_type;
    second_contained.parameters    = second.parameters;

    first_contained.flags  &= ~TYPE_ARRAY;
    second_contained.flags &= ~TYPE_ARRAY;

    return is_type_coercion_permissible(first_contained, second_contained);
}


bool
tak::array_has_inferred_sizes(const TypeData& type) {
    assert(type.flags & TYPE_ARRAY);
    for(const auto length : type.array_lengths) {
        if(length == 0) return true;
    }

    return false;
}

bool
tak::is_type_invalid_in_inferred_context(const TypeData& type) {
    return (type.kind == TYPE_KIND_PROCEDURE && !(type.flags & TYPE_POINTER)) || type.flags & TYPE_INFERRED;
}

bool
tak::is_type_reassignable(const TypeData& type) {
    return type.array_lengths.empty()
        && !(type.kind == TYPE_KIND_PROCEDURE && type.pointer_depth < 1)
        && !(type.flags & TYPE_CONSTANT);
}

bool
tak::is_returntype_lvalue_eligible(const TypeData &type) {
    return type.kind == TYPE_KIND_STRUCT
        && !(type.flags & TYPE_POINTER)
        && !(type.flags & TYPE_ARRAY);
}

bool
tak::is_type_cast_eligible(const TypeData& type) {
    return type.array_lengths.empty()
        && !(type.kind == TYPE_KIND_PROCEDURE && type.pointer_depth < 1)
        && type.kind != TYPE_KIND_STRUCT;
}

bool
tak::is_type_lop_eligible(const TypeData& type) {
    return (type.flags & TYPE_POINTER
        || type.kind == TYPE_KIND_VARIABLE)
        && type.array_lengths.empty();
}

bool
tak::is_type_bwop_eligible(const TypeData& type) {

    const auto* primitive_ptr = std::get_if<var_t>(&type.name);
    if(primitive_ptr == nullptr) {
        return false;
    }

    return type.pointer_depth == 0
        && type.kind == TYPE_KIND_VARIABLE
        && type.array_lengths.empty()
        && *primitive_ptr != VAR_VOID
        && !PRIMITIVE_IS_FLOAT(*primitive_ptr);
}

bool
tak::is_type_arithmetic_eligible(const TypeData& type, const token_t _operator) {

    assert(TOKEN_OP_IS_ARITHMETIC(_operator));

    if((type.kind == TYPE_KIND_PROCEDURE && type.pointer_depth < 2) || !type.array_lengths.empty()) {
        return false;
    }

    const auto* primitive_ptr = std::get_if<var_t>(&type.name);
    if(primitive_ptr != nullptr && *primitive_ptr == VAR_VOID && type.pointer_depth < 2) {
        return false;
    }

    if(type.flags & TYPE_POINTER) {
        return _operator == TOKEN_INCREMENT
            || _operator == TOKEN_DECREMENT
            || _operator == TOKEN_PLUS
            || _operator == TOKEN_PLUSEQ
            || _operator == TOKEN_SUB
            || _operator == TOKEN_SUBEQ;
    }

    else if(primitive_ptr == nullptr) {
        return false;
    }

    if(PRIMITIVE_IS_FLOAT(*primitive_ptr)) {
        return _operator != TOKEN_MOD && _operator != TOKEN_MODEQ;
    }

    return true;
}

bool
tak::can_operator_be_applied_to(const token_t _operator, const TypeData& type) {

    if(type.flags & TYPE_ARRAY) {
        return false;
    }

    if(_operator == TOKEN_VALUE_ASSIGNMENT) return !(type.flags & TYPE_CONSTANT) && !(type.flags & TYPE_RVALUE);
    if(TOKEN_OP_IS_ARITH_ASSIGN(_operator)) return is_type_arithmetic_eligible(type, _operator) && !(type.flags & TYPE_CONSTANT) && !(type.flags & TYPE_RVALUE);
    if(TOKEN_OP_IS_ARITHMETIC(_operator))   return is_type_arithmetic_eligible(type, _operator);
    if(TOKEN_OP_IS_BW_ASSIGN(_operator))    return is_type_bwop_eligible(type) && !(type.flags & TYPE_CONSTANT) && !(type.flags & TYPE_RVALUE);
    if(TOKEN_OP_IS_BITWISE(_operator))      return is_type_bwop_eligible(type);
    if(TOKEN_OP_IS_LOGICAL(_operator))      return is_type_lop_eligible(type);

    panic("can_operator_be_applied_to: no condition reached.");
}