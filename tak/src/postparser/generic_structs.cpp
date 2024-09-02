//
// Created by Diago on 2024-08-12.
//

#include <postparser.hpp>


static bool
postparse_try_permute_procptr_member(
    tak::TypeData& new_member,
    const std::unordered_map<std::string, tak::TypeData*>& gen_map,
    tak::Parser& parser,
    tak::SemanticErrorHandler& errs,
    const tak::LocationType& loc
) {
    assert(new_member.kind == tak::TYPE_KIND_PROCEDURE);
    if(new_member.parameters != nullptr) {
        for(size_t i = 0; i < new_member.parameters->size(); i++) {
            if(new_member.parameters->at(i).kind == tak::TYPE_KIND_PRIMITIVE) {
                continue;
            }
            else if(const auto permuted_param = postparse_try_permute_member(gen_map, new_member.parameters->at(i), parser, errs, loc)) {
                new_member.parameters->at(i) = *permuted_param;
            }
            else {
                return false;
            }
        }
    }

    if(new_member.return_type != nullptr && new_member.return_type->kind != tak::TYPE_KIND_PRIMITIVE) {
        if(const auto permuted_ret = postparse_try_permute_member(gen_map, *new_member.return_type, parser, errs, loc)) {
            *new_member.return_type = *permuted_ret;
        } else {
            return false;
        }
    }

    return true;
}


static bool
postparse_try_permute_struct_member(
    tak::TypeData& new_member,
    const std::unordered_map<std::string, tak::TypeData*>& gen_map,
    tak::Parser& parser,
    tak::SemanticErrorHandler& errs,
    const tak::LocationType& loc
) {
    assert(new_member.kind == tak::TYPE_KIND_STRUCT);

    const std::string struct_name = std::get<std::string>(new_member.name);
    const auto*       old_t       = parser.tbl_.lookup_type(struct_name);
    const size_t      used        = new_member.parameters == nullptr ? 0 : new_member.parameters->size();
    const size_t      recieves    = old_t->generic_type_names.size();

    defer_if(new_member.parameters != nullptr, [&] {
        new_member.parameters.reset();
        new_member.parameters = nullptr;
    });


    if(!used && !recieves) {
        return true;
    } if(used != recieves) {
        return false;
    }

    for(size_t i = 0; i < new_member.parameters->size(); i++) {
        if(new_member.parameters->at(i).kind == tak::TYPE_KIND_PRIMITIVE) {
            continue;
        }
        else if(const auto permuted_param = postparse_try_permute_member(gen_map, new_member.parameters->at(i), parser, errs, loc)) {
            new_member.parameters->at(i) = *permuted_param;
        }
        else {
            return false;
        }
    }


    const std::string to_str = new_member.to_string(false, false);
    if(parser.tbl_.type_exists(to_str)) {
        new_member.parameters.reset();
        new_member.parameters = nullptr;
        new_member.name = to_str;
        return true;
    }

    return postparse_try_create_permutation(to_str, parser, new_member, old_t, errs, loc);
}


std::optional<tak::TypeData>
tak::postparse_try_permute_member(
    const std::unordered_map<std::string, TypeData*>& gen_map,
    const TypeData& old_member,
    Parser& parser,
    SemanticErrorHandler& errs,
    const LocationType& loc
) {
    TypeData new_member;
    const auto* is_str = std::get_if<std::string>(&old_member.name);

    new_member.array_lengths = old_member.array_lengths;
    new_member.pointer_depth = old_member.pointer_depth;
    new_member.flags         = old_member.flags;
    new_member.parameters    = old_member.parameters;


    if(is_str != nullptr && gen_map.contains(*is_str))
    {
        const auto  gen_itr = gen_map.find(*is_str);
        const auto* gen_t   = gen_itr->second;

        if((old_member.parameters != nullptr && gen_t->parameters != nullptr)
            || (old_member.pointer_depth != 0 && gen_t->pointer_depth != 0)
            || (!old_member.array_lengths.empty() && !gen_t->array_lengths.empty())
        ) {
            errs.raise_error(fmt("Substitution failure: cannot substitute a member of type {} with {}",
                old_member.to_string(), gen_t->to_string()), loc.file, loc.pos, loc.line);
            return std::nullopt;
        }

        if(gen_t->pointer_depth)          new_member.pointer_depth = gen_t->pointer_depth;
        if(!gen_t->array_lengths.empty()) new_member.array_lengths = gen_t->array_lengths;
        if(gen_t->parameters != nullptr)  new_member.parameters    = gen_t->parameters;

        new_member.name = gen_t->name;
        new_member.kind = gen_t->kind;
    } else {
        new_member.name        = old_member.name;
        new_member.kind        = old_member.kind;
        new_member.return_type = old_member.return_type;
    }

    if(!new_member.array_lengths.empty()) new_member.flags |= TYPE_ARRAY;
    if(new_member.pointer_depth > 0)      new_member.flags |= TYPE_POINTER;


    // Substituted type is a primitive.
    if(new_member.kind == TYPE_KIND_PRIMITIVE) {
        if(new_member.parameters == nullptr) {
            return new_member;
        }
        return std::nullopt;
    }

    // Substituted type is a procedure pointer.
    if(new_member.kind == TYPE_KIND_PROCEDURE) {
        if(postparse_try_permute_procptr_member(new_member, gen_map, parser, errs, loc)) {
            return new_member;
        }
        return std::nullopt;
    }

    // Substituted type is a struct.
    if(new_member.kind == TYPE_KIND_STRUCT) {
        if(postparse_try_permute_struct_member(new_member, gen_map, parser, errs, loc)) {
            return new_member;
        }
        return std::nullopt;
    }

    return new_member;
}


bool tak::postparse_try_create_permutation(
    const std::string& name,
    Parser& parser,
    TypeData& to_conv,            // TypeData we're converting
    const UserType* old_t,        // corresponding UserType
    SemanticErrorHandler& errs,
    const LocationType& loc
) {
    assert(old_t != nullptr);
    assert(to_conv.kind == TYPE_KIND_STRUCT);
    assert(to_conv.parameters != nullptr);
    assert(to_conv.parameters->size() == old_t->generic_type_names.size());
    assert(parser.tbl_.create_type(name, {}));

    auto* new_t = parser.tbl_.lookup_type(name);
    std::unordered_map<std::string, TypeData*> gen_map;

    for(size_t i = 0; i < to_conv.parameters->size(); i++) {
        gen_map[old_t->generic_type_names[i]] = &to_conv.parameters->at(i);
    }

    for(auto& member : old_t->members) {
        if(const auto new_member = postparse_try_permute_member(gen_map, member.type, parser, errs, loc)) {
            new_t->members.emplace_back(member.name, *new_member);
        } else {
            return false;
        }
    }

    gen_map.clear();
    to_conv.name = name;
    return true;
}


void tak::postparse_inspect_struct_t(
    Parser &parser,
    TypeData& type,
    SemanticErrorHandler& errs,
    const LocationType& loc
) {
    assert(type.kind == TYPE_KIND_STRUCT);

    const auto   type_name  = std::get<std::string>(type.name);
    const auto*  utype      = parser.tbl_.lookup_type(type_name);
    const size_t receives   = utype->generic_type_names.size();
    const size_t used       = type.parameters == nullptr ? 0 : type.parameters->size();

    defer_if(type.parameters != nullptr, [&] {
        type.parameters.reset();
        type.parameters = nullptr;
    });


    if(!receives && !used) {
        return;
    }

    if(receives != used) {
        errs.raise_error(fmt("Cannot instantiate type {} with {} generic parameters (takes {}).", type_name, used, receives),
            loc.file, loc.pos, loc.line
        );
        return;
    }

    const auto type_str = type.to_string(false, false);
    if(parser.tbl_.type_exists(type_str)) {
        type.name = type_str;
    } else if(!postparse_try_create_permutation(type_str, parser, type, utype, errs, loc)) {
        errs.raise_error(fmt("Cannot instantiate type {} with these generic parameters.", type_name),
            loc.file, loc.pos, loc.line);
    }
}


void tak::postparse_inspect_proc_t(
    Parser& parser,
    const TypeData& type,
    SemanticErrorHandler& errs,
    const LocationType& loc
) {
    assert(type.kind == TYPE_KIND_PROCEDURE);

    if(type.parameters != nullptr) {
        for(auto& param : *type.parameters) {
            if(param.kind == TYPE_KIND_STRUCT) {
                postparse_inspect_struct_t(parser, param, errs, loc);
            }
            else if(param.kind == TYPE_KIND_PROCEDURE) {
                postparse_inspect_proc_t(parser, param, errs, loc);
            }
        }
    }

    if(type.return_type != nullptr) {
        if(type.return_type->kind == TYPE_KIND_STRUCT) {
            postparse_inspect_struct_t(parser, *type.return_type, errs, loc);
        }
        else if(type.return_type->kind == TYPE_KIND_PROCEDURE) {
            postparse_inspect_proc_t(parser, *type.return_type, errs, loc);
        }
    }
}


bool
tak::postparse_permute_generic_structures(Parser& parser) {
    SemanticErrorHandler errs;

    // Check symbols
    while(true) {
        Symbol* sym = nullptr;
        for(auto &[index, _sym] : parser.tbl_.sym_table_) {
            if(_sym->flags & ENTITY_POSTP_NORECHECK) continue;
            if(_sym->type.kind == TYPE_KIND_PROCEDURE || _sym->type.kind == TYPE_KIND_STRUCT) {
                sym = _sym;
            }
        }

        if(sym == nullptr) {
            break;
        }

        LocationType sym_loc;
        sym_loc.file = sym->file;
        sym_loc.pos  = sym->src_pos;
        sym_loc.line = sym->line_number;

        sym->flags |= ENTITY_POSTP_NORECHECK;
        if(sym->type.kind == TYPE_KIND_STRUCT)         postparse_inspect_struct_t(parser, sym->type, errs, sym_loc);
        else if(sym->type.kind == TYPE_KIND_PROCEDURE) postparse_inspect_proc_t(parser, sym->type, errs, sym_loc);
    }


    // check user-defined types
    while(true) {
        UserType* utype = nullptr;
        for(const auto& _utype : parser.tbl_.type_table_) {
            if(!(_utype.second->flags & ENTITY_POSTP_NORECHECK) && _utype.second->generic_type_names.empty()) {
                utype = _utype.second;
            }
        }

        if(utype == nullptr) {
            break;
        }

        LocationType type_loc;
        type_loc.file = utype->file;
        type_loc.pos  = utype->pos;
        type_loc.line = utype->line;

        utype->flags |= ENTITY_POSTP_NORECHECK;
        for(auto& member : utype->members) {
            if(member.type.kind == TYPE_KIND_PROCEDURE)   postparse_inspect_proc_t(parser, member.type, errs, type_loc);
            else if(member.type.kind == TYPE_KIND_STRUCT) postparse_inspect_struct_t(parser, member.type, errs, type_loc);
        }
    }


    // Additional AST nodes that may require inspection (cast expressions, sizeof, etc).
    for(const auto& node : parser.additional_generic_inspections_) {
        auto& type = [&]() -> TypeData& {
            if(auto* to_cast = dynamic_cast<AstCast*>(node)) {
                return to_cast->type;
            }
            else if(auto* to_sizeof = dynamic_cast<AstSizeof*>(node)) {
                auto* contained = std::get_if<TypeData>(&to_sizeof->target);
                assert(contained != nullptr && "postparse: sizeof node does not contain typedata!");
                return *contained;
            }
            panic("postparser: invalid node type stored for additional generic inspection.");
        }();

        LocationType node_loc;
        node_loc.file = node->file;
        node_loc.line = node->line;
        node_loc.pos  = node->src_pos;

        if(type.kind == TYPE_KIND_PROCEDURE)   postparse_inspect_proc_t(parser, type, errs, node_loc);
        else if(type.kind == TYPE_KIND_STRUCT) postparse_inspect_struct_t(parser, type, errs, node_loc);
    }


    errs.emit();
    return !errs.failed();
}