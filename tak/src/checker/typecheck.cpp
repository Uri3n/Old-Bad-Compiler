//
// Created by Diago on 2024-07-23.
//

#include <checker.hpp>


template<typename T>
concept arithmetic = std::integral<T> || std::floating_point<T>;

template<arithmetic T, arithmetic J>
static bool is_t_within_range(J val) {
    return val >= std::numeric_limits<T>::min() && val <= std::numeric_limits<T>::max();
}


type_data
convert_int_lit_to_type(const ast_singleton_literal* node) {

    assert(node != nullptr);
    assert(node->literal_type == TOKEN_INTEGER_LITERAL);
    assert(!node->value.empty());

    uint64_t  actual = 0;
    type_data type;

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


type_data
convert_float_lit_to_type(const ast_singleton_literal* node) {

    assert(node != nullptr);
    assert(node->literal_type == TOKEN_FLOAT_LITERAL);
    assert(!node->value.empty());

    double    actual = 0.0;
    type_data type;

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
are_array_types_equivalent(const type_data& first, const type_data& second) {

    assert(first.flags & TYPE_ARRAY);
    assert(second.flags & TYPE_ARRAY);

    if(first.array_lengths.size() != second.array_lengths.size()) {
        return false;
    }

    for(size_t i = 0; i < first.array_lengths.size(); i++) {
        if(first.array_lengths[i]) if(first.array_lengths[i] != second.array_lengths[i]) return false;
    }


    type_data first_contained;
    type_data second_contained;

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
array_has_inferred_sizes(const type_data& type) {
    assert(type.flags & TYPE_ARRAY);
    for(const auto length : type.array_lengths) {
        if(length == 0) return true;
    }

    return false;
}


std::optional<type_data>
get_bracedexpr_as_array_t(const ast_braced_expression* node, checker_context& ctx) {

    assert(node != nullptr);
    if(node->members.empty()) {
        return std::nullopt;
    }

    std::optional<type_data> contained_t;
    if(node->members[0]->type == NODE_BRACED_EXPRESSION) {
        contained_t = get_bracedexpr_as_array_t(dynamic_cast<ast_braced_expression*>(node->members[0]), ctx);
    } else {
        contained_t = visit_node(node->members[0], ctx);
    }

    if(!contained_t || is_type_invalid_in_inferred_context(*contained_t)) {
        return std::nullopt;
    }


    for(size_t i = 1; i < node->members.size(); i++) {
        if(node->members[i]->type == NODE_BRACED_EXPRESSION) {
            const auto subarray_t = get_bracedexpr_as_array_t(dynamic_cast<ast_braced_expression*>(node->members[i]), ctx);
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
            }
        }
    }

    contained_t->flags |= TYPE_ARRAY;
    contained_t->array_lengths.insert(contained_t->array_lengths.begin(), node->members.size());
    return contained_t;
}


void
check_structassign_bracedexpr(const type_data& type, const ast_braced_expression* expr, checker_context& ctx) {

    assert(expr != nullptr);
    assert(type.kind == TYPE_KIND_STRUCT);

    if(type.flags & TYPE_RVALUE) {
        ctx.raise_error(fmt("Cannot assign this braced expression to lefthand type {}.", typedata_to_str_msg(type)), expr->pos);
        return;
    }

    const std::string* name = std::get_if<std::string>(&type.name);
    assert(name != nullptr);
    std::vector<member_data>* members = ctx.parser_.lookup_type(*name);
    assert(members != nullptr);


    //
    // Validate that sizes are correct
    //

    if(members->size() != expr->members.size()) {
        ctx.raise_error(fmt("Number of elements within braced expression ({}) does not match the struct type {} ({} members).",
            expr->members.size(),
            typedata_to_str_msg(type),
            members->size()),
            expr->pos
        );

        return;
    }


    //
    // Match all members
    //

    for(size_t i = 0; i < members->size(); i++) {
        if((*members)[i].type.kind == TYPE_KIND_STRUCT && expr->members[i]->type == NODE_BRACED_EXPRESSION) {
            check_structassign_bracedexpr((*members)[i].type, dynamic_cast<ast_braced_expression*>(expr->members[i]), ctx);
            continue;
        }

        const auto element_t = visit_node(expr->members[i], ctx);
        if(!element_t) {
            ctx.raise_error(fmt("Could not deduce type of element {} in braced expression.", i + 1), expr->members[i]->pos);
            continue;
        }

        if(!is_type_coercion_permissible((*members)[i].type, *element_t)) {
            ctx.raise_error(fmt("Cannot coerce element {} of braced expression to type {} ({} was given).",
                i + 1,
                typedata_to_str_msg((*members)[i].type),
                typedata_to_str_msg(*element_t)),
                expr->members[i]->pos
            );
        }
    }
}


std::optional<type_data>
get_dereferenced_type(const type_data& type) {

    type_data deref_t = type;

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


std::optional<type_data>
get_addressed_type(const type_data& type) {

    if(type.flags & TYPE_ARRAY || type.flags & TYPE_RVALUE) {
        return std::nullopt;
    }

    type_data addressed_t = type;

    ++addressed_t.pointer_depth;
    addressed_t.flags |= TYPE_POINTER | TYPE_RVALUE;
    return addressed_t;
}


std::optional<type_data>
get_struct_member_type_data(const std::string& member_path, const std::string& base_type_name, parser& parser) {

    const auto   member_chunks = split_struct_member_path(member_path);
    const auto*  member_data   = parser.lookup_type(base_type_name);
    size_t       index         = 0;

    if(member_chunks.empty() || member_data == nullptr) {
        return std::nullopt;
    }


    std::function<std::optional<type_data>(decltype(member_data))> recurse;
    recurse = [&](const decltype(member_data) members) -> std::optional<type_data> {

        assert(member_data != nullptr);
        for(const auto& member : *members) {
            if(member.name == member_chunks[index]) {
                if(index + 1 >= member_chunks.size()) {
                    return member.type;
                }

                if(const auto* struct_name = std::get_if<std::string>(&member.type.name)) {
                    ++index;
                    if(parser.type_exists(*struct_name)) return recurse(parser.lookup_type(*struct_name));
                }

                break;
            }
        }

        return std::nullopt;
    };

    return recurse(member_data);
}


bool
types_are_identical(const type_data& first, const type_data& second) {

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
check_type_promote_non_concrete(type_data& left, const type_data& right) {

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
flip_sign(type_data& type) {

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


bool
is_type_cast_permissible(const type_data& from, const type_data& to) {

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
is_type_coercion_permissible(type_data& left, const type_data& right) {

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

    if(ptr_count == 2) return left_t != nullptr && *left_t == VAR_VOID && left.pointer_depth == 1;
    if(ptr_count != 0) return false;


    if((left_t == nullptr || right_t == nullptr) || (PRIMITIVE_IS_FLOAT(*right_t) && !PRIMITIVE_IS_FLOAT(*left_t))) {
        return false;
    }

    if(left.flags & TYPE_NON_CONCRETE) {
        return check_type_promote_non_concrete(left, right);
    }

    return var_t_to_size_bytes(*right_t) <= var_t_to_size_bytes(*left_t);
}


bool
is_type_invalid_in_inferred_context(const type_data& type) {
    return (type.kind == TYPE_KIND_PROCEDURE && !(type.flags & TYPE_POINTER)) || type.flags & TYPE_INFERRED;
}

bool
is_type_reassignable(const type_data& type) {
    return type.array_lengths.empty()
        && !(type.kind == TYPE_KIND_PROCEDURE && type.pointer_depth < 1)
        && !(type.flags & TYPE_CONSTANT);
}

bool
is_type_cast_eligible(const type_data& type) {
    return type.array_lengths.empty()
        && !(type.kind == TYPE_KIND_PROCEDURE && type.pointer_depth < 1)
        && type.kind != TYPE_KIND_STRUCT;
}

bool
is_type_lop_eligible(const type_data& type) {
    return (type.flags & TYPE_POINTER
        || type.kind == TYPE_KIND_VARIABLE)
        && type.array_lengths.empty();
}

bool
is_type_bwop_eligible(const type_data& type) {

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
is_type_arithmetic_eligible(const type_data& type, const token_t _operator) {

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

    return type.kind == TYPE_KIND_VARIABLE || (type.pointer_depth == 1 && type.kind == TYPE_KIND_STRUCT);
}

bool
can_operator_be_applied_to(const token_t _operator, const type_data& type) {

    if(type.flags & TYPE_ARRAY) {
        return false;
    }

    if(_operator == TOKEN_VALUE_ASSIGNMENT) {
        return !(type.flags & TYPE_CONSTANT) && !(type.flags & TYPE_RVALUE);
    }

    if(TOKEN_OP_IS_ARITH_ASSIGN(_operator)) {
        return is_type_arithmetic_eligible(type, _operator) && !(type.flags & TYPE_CONSTANT) && !(type.flags & TYPE_RVALUE);
    }

    if(TOKEN_OP_IS_ARITHMETIC(_operator)) {
        return is_type_arithmetic_eligible(type, _operator);
    }

    if(TOKEN_OP_IS_BW_ASSIGN(_operator)) {
        return is_type_bwop_eligible(type) && !(type.flags & TYPE_CONSTANT) && !(type.flags & TYPE_RVALUE);
    }

    if(TOKEN_OP_IS_BITWISE(_operator)) {
        return is_type_bwop_eligible(type);
    }

    if(TOKEN_OP_IS_LOGICAL(_operator)) {
        return is_type_lop_eligible(type);
    }

    panic("can_operator_be_applied_to: no condition reached.");
}

