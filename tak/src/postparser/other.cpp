//
// Created by Diago on 2024-08-12.
//

#include <postparser.hpp>


bool
tak::postparse_check_leftover_placeholders(Parser& parser) {

    SemanticErrorHandler errs;
    static constexpr std::string_view msg = "Failed to resolve {} \"{}\", first usage is here.";

    for(const auto &[_, sym] : parser.tbl_.sym_table_) {
        if(sym->flags & ENTITY_PLACEHOLDER) {
            errs.raise_error(fmt(msg, "symbol", sym->name), sym);
        }
    }

    for(const auto &[name, type] : parser.tbl_.type_table_) {
        if(type->flags & ENTITY_PLACEHOLDER) {
            errs.raise_error(fmt(msg, "type", name),
                type->file, type->pos, type->line);
        }
    }

    errs.emit();
    return !errs.failed();
}


bool
tak::postparse_check_recursive_structures(Parser& parser) {

    SemanticErrorHandler errs;
    std::function<bool(std::vector<std::string>&, const UserType*, const LocationType&)> recurse;

    recurse = [&](std::vector<std::string>& already_seen, const UserType* utype, const LocationType& loc) -> bool
    {
        assert(utype != nullptr);
        for(const auto& member : utype->members) {
            if(member.type.kind != TYPE_KIND_STRUCT || member.type.flags & TYPE_POINTER) {
                continue;
            }

            const auto name   = std::get<std::string>(member.type.name);
            const auto exists = std::find_if(already_seen.begin(), already_seen.end(), [&](const std::string& _name) {
                return _name == name;
            });

            if(exists != already_seen.end()) {
                errs.raise_error("This struct is recursive. One or more substructures occur more than once.", loc.file, loc.pos, loc.line);
                return false;
            }

            already_seen.emplace_back(name);
            return recurse(already_seen, parser.tbl_.lookup_type(name), loc);
        }

        return true;
    };

    for(const auto &[name, type] : parser.tbl_.type_table_) {
        std::vector already_seen = { name };
        recurse(already_seen, type, LocationType{ type->file, type->pos, type->line });
    }

    errs.emit();
    return !errs.failed();
}


bool
tak::postparse_delete_garbage_objects(Parser& parser) {
    for(auto it = parser.tbl_.type_table_.begin(); it != parser.tbl_.type_table_.end();) {
        if(!it->second->generic_type_names.empty()) {
            delete it->second;
            it = parser.tbl_.type_table_.erase(it);
        } else {
            ++it;
        }
    }

    return true;
}


bool
tak::postparse_verify(Parser& parser, Lexer& lxr) {
    return postparse_check_leftover_placeholders(parser)
        && postparse_permute_generic_procedures(parser, lxr)
        && postparse_permute_generic_structures(parser)
        && postparse_delete_garbage_objects(parser)
        && postparse_check_recursive_structures(parser);
}