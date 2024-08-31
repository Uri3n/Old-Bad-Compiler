//
// Created by Diago on 2024-07-30.
//

#include <checker.hpp>


bool
tak::TypeData::identical(const TypeData& first, const TypeData& second) {
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

    if(const auto* is_var = std::get_if<primitive_t>(&first.name)) {
        assert(first.parameters == nullptr && second.parameters == nullptr);
        return *is_var == std::get<primitive_t>(second.name);
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
            if(!identical((*first.parameters)[i], (*second.parameters)[i]))
                return false;
        }

    } else if(!(first.parameters == nullptr && second.parameters == nullptr)) {
        return false;
    }

    if(first.return_type != nullptr && second.return_type != nullptr) {
        return identical(*first.return_type, *second.return_type);
    }

    return first.return_type == nullptr && second.return_type == nullptr;
}


bool
tak::TypeData::is_cast_permissible(const TypeData& from, const TypeData& to) {
    if(identical(from, to)) {
        return true;
    }

    if(!from.is_cast_eligible() || !to.is_cast_eligible()) {
        return false;
    }


    const auto* primfrom_t = std::get_if<primitive_t>(&from.name);
    const auto* primto_t   = std::get_if<primitive_t>(&to.name);
    size_t      ptr_count  = 0;

    if(from.flags & TYPE_POINTER) ++ptr_count;
    if(to.flags & TYPE_POINTER)   ++ptr_count;


    if(ptr_count == 2 || ptr_count == 0) {
        return true;
    }

    if(from.flags & TYPE_POINTER && !(to.flags & TYPE_POINTER)) {
        return primto_t != nullptr && *primto_t == PRIMITIVE_U64;
    }

    if(to.flags & TYPE_POINTER && !(from.flags & TYPE_POINTER)) {
        return primfrom_t != nullptr && *primfrom_t == PRIMITIVE_U64;
    }

    return false;
}


bool
tak::TypeData::is_coercion_permissible(TypeData& left, const TypeData& right) {
    if(identical(left, right)) {
        return true;
    }

    if(!left.is_cast_eligible() || !right.is_cast_eligible()) {
        return false;
    }

    size_t      ptr_count = 0;
    const auto* left_t    = std::get_if<primitive_t>(&left.name);
    const auto* right_t   = std::get_if<primitive_t>(&right.name);

    if(left.flags  & TYPE_POINTER) ++ptr_count;
    if(right.flags & TYPE_POINTER) ++ptr_count;

    if(ptr_count == 2) return (left_t != nullptr && *left_t == PRIMITIVE_VOID && left.pointer_depth == 1) || right.flags & TYPE_NON_CONCRETE;
    if(ptr_count != 0) return false;


    if((left_t == nullptr || right_t == nullptr) || (PRIMITIVE_IS_FLOAT(*right_t) && !PRIMITIVE_IS_FLOAT(*left_t))) {
        return false;
    }

    if(left.flags & TYPE_NON_CONCRETE && !(left.flags & TYPE_POINTER) && !PRIMITIVE_IS_FLOAT(*left_t)) {
        return type_promote_non_concrete(left, right);
    }

    return primitive_t_size_bytes(*right_t) <= primitive_t_size_bytes(*left_t);
}

bool
tak::TypeData::are_arrays_equivalent(const TypeData& first, const TypeData& second) {
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

    return is_coercion_permissible(first_contained, second_contained);
}

bool
tak::TypeData::type_promote_non_concrete(TypeData& left, const TypeData& right) {
    const auto* pleft_t  = std::get_if<primitive_t>(&left.name);
    const auto* pright_t = std::get_if<primitive_t>(&right.name);
    bool is_signed       = false;

    assert(left.flags & TYPE_NON_CONCRETE);
    assert(left.pointer_depth == 0    && right.pointer_depth == 0);
    assert(left.array_lengths.empty() && right.array_lengths.empty());
    assert(pleft_t != nullptr         && pright_t != nullptr);


    if(PRIMITIVE_IS_SIGNED(*pleft_t) || PRIMITIVE_IS_SIGNED(*pright_t)) {
        is_signed = true;
    }

    if(primitive_t_size_bytes(*pright_t) > primitive_t_size_bytes(*pleft_t)) {
        left = right;
    }

    if(is_signed) {
        switch(std::get<primitive_t>(left.name)) {
            case PRIMITIVE_U8:   left.name = PRIMITIVE_I8;  break;
            case PRIMITIVE_U16:  left.name = PRIMITIVE_I16; break;
            case PRIMITIVE_U32:  left.name = PRIMITIVE_I32; break;
            case PRIMITIVE_U64:  left.name = PRIMITIVE_I64; break;
            default: break;
        }
    }

    return true;
}

bool
tak::TypeData::flip_sign() {
    assert(this->pointer_depth == 0);
    assert(this->array_lengths.empty());
    assert(this->kind == TYPE_KIND_PRIMITIVE);

    const auto* ptype = std::get_if<primitive_t>(&this->name);
    assert(ptype != nullptr);

    switch(*ptype) {
        case PRIMITIVE_U8:  this->name = PRIMITIVE_I8;  break;
        case PRIMITIVE_I8:  this->name = PRIMITIVE_U8;  break;
        case PRIMITIVE_U16: this->name = PRIMITIVE_I16; break;
        case PRIMITIVE_I16: this->name = PRIMITIVE_U16; break;
        case PRIMITIVE_U32: this->name = PRIMITIVE_I32; break;
        case PRIMITIVE_I32: this->name = PRIMITIVE_U32; break;
        case PRIMITIVE_U64: this->name = PRIMITIVE_I64; break;
        case PRIMITIVE_I64: this->name = PRIMITIVE_U64; break;
        case PRIMITIVE_BOOLEAN: return false;
        case PRIMITIVE_VOID:    return false;
        default: break;
    }

    return true;
}

bool
tak::TypeData::array_has_inferred_sizes() const {
    assert(this->flags & TYPE_ARRAY);
    for(const auto length : this->array_lengths) {
        if(length == 0) return true;
    }
    return false;
}

bool
tak::TypeData::is_invalid_in_inferred_context() const {
    return (this->kind == TYPE_KIND_PROCEDURE
        && !(this->flags & TYPE_POINTER))
        || this->flags & TYPE_INFERRED;
}

bool
tak::TypeData::is_reassignable() const {
    return this->array_lengths.empty()
        && !(this->kind == TYPE_KIND_PROCEDURE && this->pointer_depth < 1)
        && !(this->flags & TYPE_CONSTANT);
}

bool
tak::TypeData::is_returntype_lvalue_eligible() const {
    return this->kind == TYPE_KIND_STRUCT
        && !(this->flags & TYPE_POINTER)
        && !(this->flags & TYPE_ARRAY);
}

bool
tak::TypeData::is_cast_eligible() const {
    if(!this->array_lengths.empty()) {
        return false;
    }
    if(this->kind == TYPE_KIND_PROCEDURE || this->kind == TYPE_KIND_STRUCT) {
        return this->pointer_depth > 0;
    }

    return true;
}

bool
tak::TypeData::is_lop_eligible() const {
    return (this->flags & TYPE_POINTER
        || this->kind == TYPE_KIND_PRIMITIVE)
        && this->array_lengths.empty();
}

bool
tak::TypeData::is_bwop_eligible() const {
    const auto* primitive_ptr = std::get_if<primitive_t>(&this->name);
    if(primitive_ptr == nullptr) {
        return false;
    }

    return this->pointer_depth == 0
        && this->kind == TYPE_KIND_PRIMITIVE
        && this->array_lengths.empty()
        && *primitive_ptr != PRIMITIVE_VOID
        && !PRIMITIVE_IS_FLOAT(*primitive_ptr);
}

bool
tak::TypeData::is_arithmetic_eligible(const TypeData& type, const token_t _operator) {
    assert(TOKEN_OP_IS_ARITHMETIC(_operator));
    if((type.kind == TYPE_KIND_PROCEDURE && type.pointer_depth < 2) || !type.array_lengths.empty()) {
        return false;
    }

    const auto* primitive_ptr = std::get_if<primitive_t>(&type.name);
    if(primitive_ptr != nullptr && *primitive_ptr == PRIMITIVE_VOID && type.pointer_depth < 2) {
        return false;
    }

    if(type.flags & TYPE_POINTER) {
        return TOKEN_OP_IS_VALID_PTR_ARITH(_operator);
    }

    else if(primitive_ptr == nullptr) {
        return false;
    }

    if(PRIMITIVE_IS_FLOAT(*primitive_ptr)) {
        return _operator != TOKEN_MOD && _operator != TOKEN_MODEQ;
    }

    return true;
}

std::optional<tak::TypeData>
tak::TypeData::get_lowest_array_type(const TypeData& type) {
    if(!(type.flags & TYPE_ARRAY)) {
        return std::nullopt;
    }

    std::optional to_ret = type;
    to_ret->array_lengths.clear();
    to_ret->flags &= ~TYPE_ARRAY;
    return to_ret;
}

std::optional<tak::TypeData>
tak::TypeData::get_contained(const TypeData& type) {
    TypeData deref_t = type;

    if(deref_t.flags & TYPE_ARRAY) {
        assert(!deref_t.array_lengths.empty());
        deref_t.array_lengths.pop_back();

        if(deref_t.array_lengths.empty()) {
            deref_t.flags &= ~TYPE_ARRAY;
        }
    } else if(deref_t.flags & TYPE_POINTER) {
        assert(deref_t.pointer_depth > 0);
        --deref_t.pointer_depth;

        if(!deref_t.pointer_depth) {
            deref_t.flags &= ~TYPE_POINTER;
        }
    } else {
        return std::nullopt;
    }


    if(deref_t.kind == TYPE_KIND_PROCEDURE && !(deref_t.flags & TYPE_POINTER)) {
        return std::nullopt;
    } if(const auto* is_primitive = std::get_if<primitive_t>(&deref_t.name)) {
        if(*is_primitive == PRIMITIVE_VOID && !(deref_t.flags & TYPE_POINTER)) {
            return std::nullopt;
        }
    } if(deref_t.flags & TYPE_RVALUE) {
        deref_t.flags &= ~TYPE_RVALUE;
    }

    return deref_t;
}

std::optional<tak::TypeData>
tak::TypeData::get_pointer_to(const TypeData& type) {
    if(type.flags & TYPE_ARRAY || type.flags & TYPE_RVALUE) {
        return std::nullopt;
    }

    TypeData addressed_t = type;

    ++addressed_t.pointer_depth;
    addressed_t.flags |= TYPE_POINTER | TYPE_RVALUE;
    return addressed_t;
}

bool
tak::TypeData::can_operator_be_applied_to(const token_t _operator, const TypeData& type) {
    if(type.flags & TYPE_ARRAY) {
        return false;
    }

    if(_operator == TOKEN_VALUE_ASSIGNMENT) {
        return !(type.flags & TYPE_CONSTANT)
            && !(type.flags & TYPE_RVALUE);
    }

    if(TOKEN_OP_IS_ARITH_ASSIGN(_operator)) {
        return is_arithmetic_eligible(type, _operator)
            && !(type.flags & TYPE_CONSTANT)
            && !(type.flags & TYPE_RVALUE);
    }

    if(TOKEN_OP_IS_ARITHMETIC(_operator)) {
        return is_arithmetic_eligible(type, _operator);
    }

    if(TOKEN_OP_IS_BW_ASSIGN(_operator)) {
        return type.is_bwop_eligible()
            && !(type.flags & TYPE_CONSTANT)
            && !(type.flags & TYPE_RVALUE);
    }

    if(TOKEN_OP_IS_BITWISE(_operator)) {
        return type.is_bwop_eligible();
    }

    if(TOKEN_OP_IS_LOGICAL(_operator)) {
        return type.is_lop_eligible();
    }

    panic("can_operator_be_applied_to: no condition reached.");
}