//
// Created by Diago on 2024-07-23.
//

#include <checker.hpp>


std::optional<type_data>
get_struct_member_type_data(const std::string& member_path, const std::string& base_type_name, parser& parser) {

    const auto   member_chunks = split_struct_member_path(member_path);
    const auto*  member_data   = parser.lookup_type(base_type_name);
    size_t       index         = 0;

    if(member_chunks.empty() || member_data == nullptr)
        return std::nullopt;


    std::function<type_data(decltype(member_data))> recurse;
    recurse = [&](const decltype(member_data) members) {

        assert(member_data != nullptr);
        for(const auto& member : *members) {
            if(member.name == member_chunks[index]) {

                if(index + 1 >= member_chunks.size()) {
                    return member.type;
                }

                if(const auto* struct_name = std::get_if<std::string>(&member.type.name)) {
                    print("MEMBERS OF: {}", *struct_name);
                    ++index;
                    return recurse(parser.lookup_type(*struct_name));
                }

                break;
            }
        }

        // Should never occur. This gets validated by the parser.
        panic("get_struct_member_type_data: accessing invalid struct member with '.' operator.");
    };

    return recurse(member_data);
}


bool
types_are_identical(const type_data& first, const type_data& second) {

    if(first.sym_type != second.sym_type
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
is_type_coercion_permissible(const type_data& left, const type_data& right) {

    if(types_are_identical(left, right))
        return true;

    if(!is_type_cast_eligible(left) || !is_type_cast_eligible(right))
        return false;


    size_t      ptr_count = 0;
    const auto* left_t    = std::get_if<var_t>(&left.name);
    const auto* right_t   = std::get_if<var_t>(&right.name);

    if(left.flags  & TYPE_IS_POINTER) ++ptr_count;
    if(right.flags & TYPE_IS_POINTER) ++ptr_count;

    if(ptr_count == 2) return left_t != nullptr && *left_t == VAR_VOID && left.pointer_depth == 1;
    if(ptr_count != 0) return false;


    if(left_t == nullptr || right_t == nullptr)                        // one or more types are not primitive
        return false;

    if(PRIMITIVE_IS_FLOAT(*right_t) && !PRIMITIVE_IS_FLOAT(*left_t))   // float -> integer : disallowed without cast
        return false;

    size_t signed_cnt = 0;
    if(PRIMITIVE_IS_SIGNED(*left_t))  ++signed_cnt;
    if(PRIMITIVE_IS_SIGNED(*right_t)) ++signed_cnt;

    if(signed_cnt != 2 && signed_cnt != 0)                             // signed -> unsigned : disallowed without cast
        return false;


    return var_t_to_size_bytes(*right_t) >= var_t_to_size_bytes(*left_t);
}


bool
is_type_reassignable(const type_data& type) {
    return type.array_lengths.empty()
        && !(type.sym_type == TYPE_KIND_PROCEDURE && type.flags & TYPE_IS_POINTER)
        && !(type.flags & TYPE_IS_CONSTANT);
}

bool
is_type_cast_eligible(const type_data& type) {
    return type.array_lengths.empty()
        && !(type.sym_type == TYPE_KIND_PROCEDURE && type.flags & TYPE_IS_POINTER)
        && type.sym_type != TYPE_KIND_STRUCT;
}


