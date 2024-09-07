//
// Created by Diago on 2024-07-20.
//

#include <parser.hpp>


tak::AstNode*
tak::parse_type_alias(Parser& parser, Lexer& lxr) {
    assert(lxr.current().value == "alias");

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
tak::parse_include(Parser& parser, Lexer& lxr) {
    assert(lxr.current().value == "include");

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
tak::parse_visibility_extern(Parser& parser, Lexer& lxr) {
    assert(lxr.current().value == "extern");

    const size_t   pos  = lxr.current().src_pos;
    const uint32_t line = lxr.current().line;
    const uint32_t flag = [&]() -> uint32_t {
        const Token peeked = lxr.peek(1);
        if(peeked != TOKEN_STRING_LITERAL) {
            return ENTITY_FOREIGN;
        }
        if(peeked.value != "\"C\"") {
            lxr.raise_error(R"(Invalid string literal after "extern" directive.)");
            return ENTITY_FLAGS_NONE;
        }

        lxr.advance(1);
        return ENTITY_FOREIGN_C;
    }();

    //
    // Validate.
    //

    lxr.advance(1);
    if(flag == ENTITY_FLAGS_NONE) {
        lxr.raise_error(R"(Invalid "extern" directive.)", pos, line);
        return nullptr;
    }
    if(flag == ENTITY_FOREIGN_C && !parser.tbl_.namespace_stack_.empty()) {
        lxr.raise_error(R"(Cannot use extern "C" inside of a namespace.)", pos, line);
        return nullptr;
    }
    if(parser.tbl_.scope_stack_.size() > 1) {
        lxr.raise_error(R"(Cannot use "extern" directive at non-global scope.)", pos, line);
        return nullptr;
    }

    //
    // Parse node.
    //

    bool  state = false;
    auto* node  = parse(parser, lxr, true);

    if(node == nullptr) {
        return nullptr;
    }

    defer_if(!state, [&] {
        delete node;
    });

    //
    // Find the symbol.
    //

    Symbol* sym = [&]() -> Symbol* {
        if(const auto* vardecl = dynamic_cast<const AstVardecl*>(node)) {
            return parser.tbl_.lookup_unique_symbol(vardecl->identifier->symbol_index);
        }
        else if(const auto* procdecl = dynamic_cast<const AstProcdecl*>(node)) {
            return parser.tbl_.lookup_unique_symbol(procdecl->identifier->symbol_index);
        }

        lxr.raise_error("Expected next expression to be a variable or procedure declaration.", pos, line);
        return nullptr;
    }();

    if(sym == nullptr) {
        return nullptr;
    }

    if(!sym->generic_type_names.empty()) {
        lxr.raise_error("Cannot apply \"extern\" directive to a symbol with generic parameters.", pos, line);
        return nullptr;
    }

    if(sym->flags & ENTITY_FOREIGN) {
        sym->flags &= ~ENTITY_FOREIGN;
    }

    sym->flags |= flag;
    state = true;
    return node;
}


tak::AstNode*
tak::parse_visibility_intern(Parser& parser, Lexer& lxr) {
    assert(lxr.current().value == "intern");
    const size_t   pos  = lxr.current().src_pos;
    const uint32_t line = lxr.current().line;

    lxr.advance(1);
    if(parser.tbl_.scope_stack_.size() > 1) {
        lxr.raise_error(R"(Cannot use "intern" directive at non-global scope.)", pos, line);
        return nullptr;
    }

    bool  state = false;
    auto* node  = parse(parser, lxr, true);

    if(node == nullptr) {
        return nullptr;
    }
    defer_if(!state, [&] {
        delete node;
    });


    Symbol* sym = [&]() -> Symbol* {
        if(const auto* vardecl = dynamic_cast<const AstVardecl*>(node)) {
            return parser.tbl_.lookup_unique_symbol(vardecl->identifier->symbol_index);
        }
        else if(const auto* procdecl = dynamic_cast<const AstProcdecl*>(node)) {
            return parser.tbl_.lookup_unique_symbol(procdecl->identifier->symbol_index);
        }

        lxr.raise_error("Expected next expression to be a variable or procedure declaration.", pos, line);
        return nullptr;
    }();

    if(!sym->generic_type_names.empty()) {
        const std::string msg = R"(redundant "intern" directive, generic symbols are implied to be internal.)";
        if(Config::get().flags() & CONFIG_WARN_IS_ERR) {
            lxr.raise_error(msg, pos, line);
            return nullptr;
        }
        lxr.raise_warning(msg, pos, line);
    }

    sym->flags |= ENTITY_INTERNAL;
    state = true;
    return node;
}


tak::AstNode*
tak::parse_compiler_directive(Parser& parser, Lexer& lxr) {
    assert(lxr.current() == TOKEN_AT);

    lxr.advance(1);
    if(lxr.current() != TOKEN_IDENTIFIER) {
        lxr.raise_error("Expected directive name.");
    }

    if(lxr.current().value == "alias")    return parse_type_alias(parser, lxr);
    if(lxr.current().value == "include")  return parse_include(parser, lxr);
    if(lxr.current().value == "intern")   return parse_visibility_intern(parser, lxr);
    if(lxr.current().value == "extern")   return parse_visibility_extern(parser, lxr);

    lxr.raise_error("Invalid compiler directive.");
    return nullptr;
}
