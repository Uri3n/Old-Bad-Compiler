//
// Created by Diago on 2024-07-30.
//

#include <checker.hpp>


std::optional<TypeData>
get_bracedexpr_as_array_t(const AstBracedExpression* node, CheckerContext& ctx) {

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
            }
        }
    }

    contained_t->flags |= TYPE_ARRAY;
    contained_t->array_lengths.insert(contained_t->array_lengths.begin(), node->members.size());
    return contained_t;
}


std::optional<TypeData>
get_dereferenced_type(const TypeData& type) {

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


std::optional<TypeData>
get_addressed_type(const TypeData& type) {

    if(type.flags & TYPE_ARRAY || type.flags & TYPE_RVALUE) {
        return std::nullopt;
    }

    TypeData addressed_t = type;

    ++addressed_t.pointer_depth;
    addressed_t.flags |= TYPE_POINTER | TYPE_RVALUE;
    return addressed_t;
}


std::optional<TypeData>
get_struct_member_type_data(const std::string& member_path, const std::string& base_type_name, Parser& parser) {

    const auto   member_chunks = split_struct_member_path(member_path);
    const auto*  member_data   = parser.lookup_type(base_type_name);
    size_t       index         = 0;

    if(member_chunks.empty() || member_data == nullptr) {
        return std::nullopt;
    }


    std::function<std::optional<TypeData>(decltype(member_data))> recurse;
    recurse = [&](const decltype(member_data) members) -> std::optional<TypeData> {

        assert(member_data != nullptr);
        for(const auto& member : *members) {
            if(member.name == member_chunks[index]) {
                if(index + 1 >= member_chunks.size()) {
                    return member.type;
                }

                if(const auto* struct_name = std::get_if<std::string>(&member.type.name)) {
                    ++index;
                    if(parser.type_exists(*struct_name) && member.type.array_lengths.empty() && member.type.pointer_depth < 2) {
                        return recurse(parser.lookup_type(*struct_name));
                    }
                }

                break;
            }
        }

        return std::nullopt;
    };

    return recurse(member_data);
}


void
assign_bracedexpr_to_struct(const TypeData& type, const AstBracedExpression* expr, CheckerContext& ctx) {

    assert(expr != nullptr);
    assert(type.kind == TYPE_KIND_STRUCT);

    if(type.flags & TYPE_RVALUE) {
        ctx.raise_error(fmt("Cannot assign this braced expression to lefthand type {}.", typedata_to_str_msg(type)), expr->pos);
        return;
    }

    const std::string* name = std::get_if<std::string>(&type.name);
    assert(name != nullptr);
    std::vector<MemberData>* members = ctx.parser_.lookup_type(*name);
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
            assign_bracedexpr_to_struct((*members)[i].type, dynamic_cast<AstBracedExpression*>(expr->members[i]), ctx);
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