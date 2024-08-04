//
// Created by Diago on 2024-08-03.
//

#include <parser.hpp>


static bool
compose_add_type_method(tak::UserType* type, const std::string& type_name, tak::Symbol* proc, tak::Lexer& lxr) {

    assert(type != nullptr);
    assert(proc != nullptr);

    if(proc->type.parameters == nullptr || proc->type.parameters->empty()) {
        return true;
    }

    const auto& first       = proc->type.parameters->front();
    const auto* name        = std::get_if<std::string>(&first.name);
    const auto  method_name = tak::split_string(proc->name, '\\').back();

    if(name != nullptr && *name == type_name && first.pointer_depth == 1 && first.array_lengths.empty()) {
        for(const auto& member : type->members) {
            if(member.name == method_name) {
                lxr.raise_error(tak::fmt("Cannot create method {} because type {} already has a member of the same name.",
                    method_name, type_name));

                return false;
            }
        }

        type->members.emplace_back(tak::MemberData{ method_name, {} });
        type->members.back().type.sym_ref = proc->symbol_index;
        proc->type.flags |= tak::TYPE_PROC_METHOD;
    }

    return true;
}


tak::AstNode*
tak::parse_compose(Parser& parser, Lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_KW_COMPOSE, "Expected \"compose\" keyword.");


    if(!parser.namespace_stack_.empty()) {
        lxr.raise_error("Cannot use \"compose\" within a namespace.");
        return nullptr;
    }

    if(parser.scope_stack_.size() > 1) {
        lxr.raise_error("Use of \"compose\" at non-global scope.");
        return nullptr;
    }

    lxr.advance(1);
    if(!TOKEN_IDENT_START(lxr.current().type)) {
        lxr.raise_error("Expected struct name.");
        return nullptr;
    }


    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;

    bool  state     = false;
    bool  once      = false;

    auto* node      = new AstComposeDecl();
    node->pos       = curr_pos;
    node->type_name = get_namespaced_identifier(lxr).value_or(std::string());

    std::vector<std::string> chunks;
    UserType* members = nullptr;

    defer([&] {
        if(!state) { delete node; }
        for(size_t i = 0; i < chunks.size(); ++i) { parser.leave_namespace(); }
    });


    if(node->type_name.empty()) {
        return nullptr;
    }

    if(node->type_name.front() != '\\') {
        node->type_name.insert(0, "\\");
    }

    if(parser.type_exists(node->type_name)) {
        members = parser.lookup_type(node->type_name);
    }
    else if(parser.type_alias_exists(node->type_name)) {
        lxr.raise_error("Type aliases cannot be used with \"compose\".");
        return nullptr;
    }
    else {
        assert(parser.create_placeholder_type(node->type_name, curr_pos, line));
        members = parser.lookup_type(node->type_name);
    }

    chunks = split_string(node->type_name, '\\');
    for(const auto& chunk : chunks) {
        if(!parser.enter_namespace(chunk)) {
            lxr.raise_error(fmt("namespace {} within {} has already been entered.", chunk, node->type_name), curr_pos, line);
            return nullptr;
        }
    }


    lxr.advance(1);
    if(lxr.current() != TOKEN_LBRACE) {
        once = true;
    } else {
        lxr.advance(1);
    }

    while(lxr.current() != TOKEN_RBRACE || once) {
        const auto& child = node->children.emplace_back(parse_expression(parser, lxr, false));
        if(child == nullptr) {
            return nullptr;
        }

        if(!VALID_WITHIN_COMPOSE(child->type)) {
            lxr.raise_error("Expression cannot be used within \"compose\" block.", child->pos);
            return nullptr;
        }

        if(child->type == NODE_PROCDECL) {
            const auto* proc = dynamic_cast<AstProcdecl*>(child);
            assert(proc != nullptr);
            assert(proc->identifier != nullptr);
            if(!compose_add_type_method(members, node->type_name, parser.lookup_unique_symbol(proc->identifier->symbol_index), lxr)) {
                return nullptr;
            }
        }

        if(once) {
            state = true;
            return node;
        }
    }

    lxr.advance(1);
    state = true;
    return node;
}
