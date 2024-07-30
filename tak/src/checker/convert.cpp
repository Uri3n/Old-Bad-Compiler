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


TypeData
convert_int_lit_to_type(const AstSingletonLiteral* node) {

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


TypeData
convert_float_lit_to_type(const AstSingletonLiteral* node) {

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


bool
type_promote_non_concrete(TypeData& left, const TypeData& right) {

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
flip_sign(TypeData& type) {

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