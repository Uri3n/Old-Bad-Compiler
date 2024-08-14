//
// Created by Diago on 2024-07-20.
//

#include <parser.hpp>


tak::AstNode*
tak::parse_type_alias(Parser& parser, Lexer& lxr) {

    parser_assert(lxr.current().value == "alias", "Expected \"@alias\" directive.");

    if(parser.tbl_.scope_stack_.size() > 1) {
        lxr.raise_error("Type alias definition at non-global scope.");
        return nullptr;
    }

    lxr.advance(1);
    if(lxr.current() != TOKEN_IDENTIFIER) {
        lxr.raise_error("Expected alias name.");
        return nullptr;
    }

    auto* node   = new AstTypeAlias(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);
    bool  state  = false;

    defer_if(!state, [&] {
       delete node;
    });


    node->name = parser.tbl_.namespace_as_string() + std::string(lxr.current().value);
    if(parser.tbl_.type_alias_exists(node->name) || parser.tbl_.type_exists(node->name)) {
        lxr.raise_error("Type or type alias with the same name already exists within this namespace.");
        return nullptr;
    }

    if(lxr.peek(1) != TOKEN_VALUE_ASSIGNMENT) {
        lxr.raise_error("Expected '=' after type alias name.");
        return nullptr;
    }

    lxr.advance(2);
    if(!TOKEN_IDENT_START(lxr.current().type) && lxr.current().kind != KIND_TYPE_IDENTIFIER) {
        lxr.raise_error("Expected type identifier.");
        return nullptr;
    }

    if(const auto type = parse_type(parser,lxr)) { // create the type alias.
        parser.tbl_.create_type_alias(node->name, *type);
        state = true;
        return node;
    }

    return nullptr;
}


tak::AstNode*
tak::parse_callconv(Parser& parser, Lexer& lxr) {

    parser_assert(lxr.current().value == "callconv", "Expected \"callconv\".");
    lxr.advance(1);

    uint32_t sym_flag = ENTITY_FLAGS_NONE;

    if(lxr.current() != TOKEN_STRING_LITERAL) {
        lxr.raise_error("Expected calling convention.");
        return nullptr;
    }

    if(lxr.current().value == "\"C\"") { // Only one for now. More can be added later.
        sym_flag = ENTITY_CALLCONV_C;
    } else {
        lxr.raise_error("Unrecognized calling convention.");
        return nullptr;
    }


    lxr.advance(1);
    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;

    auto* node  = parse_expression(parser, lxr, false);
    bool  state = false;

    defer_if(!state, [&] {
       delete node;
    });


    if(node == nullptr) {
        return nullptr;
    }

    if(node->type != NODE_PROCDECL) {
        lxr.raise_error("Expected procedure declaration after \"callconv\" statement.", curr_pos, line);
        return nullptr;
    }

    const auto* pdecl = dynamic_cast<AstProcdecl*>(node);
    auto*       sym   = parser.tbl_.lookup_unique_symbol(pdecl->identifier->symbol_index);

    sym->flags |= sym_flag;
    state = true;
    return node;
}


tak::AstNode*
tak::parse_include(Parser& parser, Lexer& lxr) {

    parser_assert(lxr.current().value == "include", "Expected \"include\" directive.");

    if(parser.tbl_.scope_stack_.size() > 1) {
        lxr.raise_error("Include statement at non-global scope.");
        return nullptr;
    }

    lxr.advance(1);
    if(lxr.current() != TOKEN_STRING_LITERAL) {
        lxr.raise_error("Expected string literal file path (e.g. \"path/to/file\").");
        return nullptr;
    }


    assert(lxr.current().value.size() >= 2);

    bool  state    = false;
    auto  path_str = std::string(lxr.current().value);
    auto* node     = new AstIncludeStmt(lxr.current().src_pos, lxr.current().line, lxr.source_file_name_);

    defer_if(!state, [&] {
        delete node;
    });


    path_str.erase(0,1);
    path_str.erase(path_str.size() - 1);

    if(path_str.empty()) {
        lxr.raise_error("No file path provided.");
        return nullptr;
    }

    constexpr std::string_view fs_err_msg =
        "{}: Encountered native filesystem exception attempting to parse this include path.\n"
        " - Involved paths: {}, {}\n"
        " - Code: {}";


    try {
        const auto as_abs = std::filesystem::absolute(path_str);
        const auto as_rel = std::filesystem::path(lxr.source_file_name_).parent_path() / path_str;

        if(exists(as_abs) && is_regular_file(as_abs))      node->name = as_abs.string();
        else if(exists(as_rel) && is_regular_file(as_rel)) node->name = absolute(as_rel);
        else                                               throw std::runtime_error("file does not exist.");

        const auto include_itr = std::find_if(parser.included_files_.begin(), parser.included_files_.end(), [&](const IncludedFile& f) {
            return f.name == node->name;
        });

        if(include_itr == parser.included_files_.end()) {
            parser.included_files_.emplace_back(IncludedFile{node->name, INCLUDE_STATE_PENDING});
        }
    }
    catch(const std::filesystem::filesystem_error& e) {
        lxr.raise_error(fmt(fs_err_msg, e.what(), e.path1().string(), e.path2().string(), e.code().message()));
        return nullptr;
    }
    catch(const std::runtime_error& e) {
        lxr.raise_error(fmt("Could not include \"{}\": {}", path_str, e.what()));
        return nullptr;
    }
    catch(...) {
        print("Could not include this file path. It is likely malformed.");
        return nullptr;
    }


    lxr.advance(1);
    state = true;
    return node;
}


tak::AstNode*
tak::parse_compiler_directive(Parser& parser, Lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_AT, "Expected '@'.");
    lxr.advance(1);

    if(lxr.current() != TOKEN_IDENTIFIER) {
        lxr.raise_error("Expected directive name.");
    }

    if(lxr.current().value == "alias")    return parse_type_alias(parser, lxr);
    if(lxr.current().value == "callconv") return parse_callconv(parser, lxr);
    if(lxr.current().value == "include")  return parse_include(parser, lxr);

    lxr.raise_error("Invalid compiler directive.");
    return nullptr;
}
