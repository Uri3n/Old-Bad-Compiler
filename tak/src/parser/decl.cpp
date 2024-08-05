//
// Created by Diago on 2024-07-09.
//

#include <parser.hpp>


tak::AstNode*
tak::parse_proc_ptr(Symbol* proc, Parser& parser, Lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_KW_PROC, "Expected \"proc\" keyword.");
    parser_assert(lxr.peek(1) == TOKEN_BITWISE_XOR_OR_PTR, "Expected next token to be pointy fella (^).");
    parser_assert(proc != nullptr, "Null symbol pointer.");


    //
    // Evaluate type
    //

    if(const auto type = parse_type(parser, lxr)) {
        proc->type = *type;
    } else {
        return nullptr;
    }


    //
    // Create AST node, check for additional assignment
    //

    auto* node       = new AstVardecl();
    node->identifier = new AstIdentifier();
    node->pos        = lxr.current().src_pos;
    bool state       = false;

    node->identifier->parent       = node;
    node->identifier->symbol_index = proc->symbol_index;
    node->identifier->pos          = lxr.current().src_pos;

    defer_if(!state, [&] {
        delete node;
    });


    //
    // Parse assignment if it exists, otherwise leave as default initialized.
    //

    if(lxr.current() == TOKEN_VALUE_ASSIGNMENT) {

        const size_t   curr_pos  = lxr.current().src_pos;
        const uint32_t curr_line = lxr.current().line;


        lxr.advance(1);
        node->init_value = parse_expression(parser, lxr, true);
        if(*(node->init_value) == nullptr) {
            return nullptr;
        }


        const auto subexpr_type = (*node->init_value)->type;
        if(!VALID_SUBEXPRESSION(subexpr_type)) {
            lxr.raise_error("Invalid expression being assigned to variable.", curr_pos, curr_line);
            return nullptr;
        }

        state = true;
        return node;
    }


    if(proc->flags & SYM_GLOBAL) {
        proc->type.flags &= ~TYPE_UNINITIALIZED;
    }

    proc->type.flags |= TYPE_DEFAULT_INIT;
    state = true;
    return node;
}


tak::AstVardecl*
tak::parse_parameterized_vardecl(Parser& parser, Lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_IDENTIFIER, "Expected variable identifier.");

    const auto     name      = parser.namespace_as_string() + std::string(lxr.current().value);
    const size_t   src_pos   = lxr.current().src_pos;
    const uint32_t line      = lxr.current().line;
    uint64_t       flags     = TYPE_PROCARG;


    if(parser.namespace_exists(std::string(lxr.current().value))) {
        lxr.raise_error("Confusion: variable has the same name as a namespace it is declared in.");
        return nullptr;
    }

    if(parser.scoped_symbol_exists_at_current_scope(name)) {   // a new scope should be pushed by tak::parse_procdecl
        lxr.raise_error("Symbol already exists within this scope.");
        return nullptr;
    }

    lxr.advance(1);
    if(lxr.current() == TOKEN_CONST_TYPE_ASSIGNMENT) {
        flags |= TYPE_CONSTANT;
    }

    else if(lxr.current() != TOKEN_TYPE_ASSIGNMENT) {
        lxr.raise_error("Expected type assignment.");
        return nullptr;
    }

    lxr.advance(1);
    if(lxr.current().kind != KIND_TYPE_IDENTIFIER && !TOKEN_IDENT_START(lxr.current().type)) {
        lxr.raise_error("Expected type identifier.");
        return nullptr;
    }


    //
    // Once we parse the type we need to make sure that it isn't a procedure or an array.
    //

    const auto _type_data = parse_type(parser, lxr);
    if(!_type_data) {
        return nullptr;
    }

    if(_type_data->kind == TYPE_KIND_PROCEDURE && _type_data->pointer_depth < 1) {
        lxr.raise_error("Procedures cannot be procedure parameters. Pass a pointer instead.");
        return nullptr;
    }

    if(!_type_data->array_lengths.empty()) {
        lxr.raise_error("Arrays cannot be procedure parameters. Pass an array as a pointer instead.");
        return nullptr;
    }


    //
    // Store the parameter as a symbol in the symbol table.
    //

    const auto* var_ptr = parser.create_symbol(name, src_pos, line, _type_data->kind, flags, _type_data);
    if(var_ptr == nullptr) {
        return nullptr;
    }

    auto* vardecl        = new AstVardecl();
    vardecl->identifier  = new AstIdentifier();
    vardecl->pos         = src_pos;

    vardecl->identifier->parent       = vardecl;
    vardecl->identifier->symbol_index = var_ptr->symbol_index;
    vardecl->identifier->pos          = src_pos;

    return vardecl;
}


tak::AstNode*
tak::parse_procdecl(Symbol* proc, Parser& parser, Lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_KW_PROC, "Expected proc type identifier.");

    if(!(proc->flags & SYM_GLOBAL)) {
        lxr.raise_error("Declaration of procedure at non-global scope.");
        return nullptr;
    }

    if(!(proc->type.flags & TYPE_CONSTANT)) {
        lxr.raise_error("Procedures must be declared as constant. This one was declared using ':'.");
        return nullptr;
    }


    //
    // Create AST node
    //

    parser.push_scope();

    auto* node         = new AstProcdecl();
    node->identifier   = new AstIdentifier();
    node->pos          = lxr.current().src_pos;

    node->identifier->symbol_index = proc->symbol_index;
    node->identifier->parent       = node;
    node->identifier->pos          = lxr.current().src_pos;

    bool state = false;
    defer([&] {
        if(!state) { delete node; }
        parser.pop_scope();
    });


    //
    // Get procedure parameters (if any)
    //

    lxr.advance(1);
    if(lxr.current() != TOKEN_LPAREN) {
        lxr.raise_error("Expected parameter list here.");
        return nullptr;
    }

    lxr.advance(1);
    while(lxr.current() != TOKEN_RPAREN) {

        if(lxr.current() != TOKEN_IDENTIFIER) {
            lxr.raise_error("Expected procedure parameter.");
            return nullptr;
        }

        auto* param = parse_parameterized_vardecl(parser, lxr);
        if(param == nullptr) {
            return nullptr;
        }

        param->parent = node;
        node->parameters.emplace_back(param);

        if(lxr.current() == TOKEN_COMMA) {
            lxr.advance(1);
        }
    }


    //
    // Get return type.
    //

    lxr.advance(1);
    if(lxr.current() == TOKEN_ARROW && (lxr.peek(1).kind == KIND_TYPE_IDENTIFIER || TOKEN_IDENT_START(lxr.peek(1)))) {
        lxr.advance(1);
        if(lxr.current() == TOKEN_KW_VOID && lxr.peek(1) != TOKEN_BITWISE_XOR_OR_PTR) {
            lxr.advance(1);
        } else if(const auto _type_data = parse_type(parser, lxr)) {
            proc->type.return_type  = std::make_shared<TypeData>();
            *proc->type.return_type = *_type_data;
        } else {
            return nullptr;
        }
    }


    //
    // Store params in the symbol table
    //

    if(!node->parameters.empty()) {
        proc->type.parameters = std::make_shared<std::vector<TypeData>>();
    }

    for(const auto& param : node->parameters) {
        const auto* symbol_ptr = parser.lookup_unique_symbol(param->identifier->symbol_index);
        if(symbol_ptr == nullptr) {
            return nullptr;
        }

        proc->type.parameters->emplace_back(symbol_ptr->type);
    }


    //
    // If no body, consider the proc as a foreign import.
    //

    if(lxr.current() == TOKEN_SEMICOLON || lxr.current() == TOKEN_COMMA) {
        proc->flags |= SYM_FOREIGN;
        lxr.advance(1);
        state = true;
        return node;
    }


    //
    // Now we parse the rest of the procedure body. Just keep calling parse_expression
    // and adding it to the procedure body.
    //

    if(lxr.current() != TOKEN_LBRACE) {
        lxr.raise_error("Expected start of procedure body here.");
        return nullptr;
    }

    lxr.advance(1);
    while(lxr.current() != TOKEN_RBRACE) {
        auto* expr = parse_expression(parser, lxr, false);
        if(expr == nullptr) {
            return nullptr;
        }

        expr->parent = node;
        node->body.emplace_back(expr);
    }


    lxr.advance(1);
    state = true;
    return node;
}


tak::AstNode*
tak::parse_vardecl(Symbol* var, Parser& parser, Lexer& lxr) {

    parser_assert(lxr.current().kind == KIND_TYPE_IDENTIFIER, "Expected type identifier.");

    const auto type_data = parse_type(parser, lxr);
    if(!type_data) {
        return nullptr;
    }

    const uint64_t temp = var->type.flags;
    var->type           = *type_data;
    var->type.flags    |= temp;


    //
    // Generate AST node
    //

    bool  state       = false;
    auto* node        = new AstVardecl();
    node->identifier  = new AstIdentifier();
    node->pos         = lxr.current().src_pos;

    node->identifier->symbol_index = var->symbol_index;
    node->identifier->parent       = node;
    node->identifier->pos          = lxr.current().src_pos;

    defer_if(!state, [&] {
        delete node;
    });


    if(lxr.current() == TOKEN_VALUE_ASSIGNMENT) {

        const size_t   curr_pos  = lxr.current().src_pos;
        const uint32_t curr_line = lxr.current().line;


        lxr.advance(1);
        node->init_value = parse_expression(parser, lxr, true);
        if(*(node->init_value) == nullptr) {
            return nullptr;
        }

        const auto subexpr_type = (*node->init_value)->type;
        if(!VALID_SUBEXPRESSION(subexpr_type)) {
            lxr.raise_error("Invalid expression being assigned to variable.", curr_pos, curr_line);
            return nullptr;
        }

        state = true;
        return node;
    }

    if(var->flags & SYM_GLOBAL) {
        var->type.flags &= ~TYPE_UNINITIALIZED;
    }

    var->type.flags |= TYPE_DEFAULT_INIT;
    state = true;
    return node;
}


tak::AstNode*
tak::parse_usertype_decl(Symbol* sym, Parser& parser, Lexer& lxr) {

    parser_assert(TOKEN_IDENT_START(lxr.current().type), "Expected user type name.");

    if(const auto type = parse_type(parser, lxr)) {
        const uint64_t temp  = sym->type.flags;
        sym->type            = *type;
        sym->type.flags     |= temp;
    } else {
        return nullptr;
    }


    auto* node                     = new AstVardecl();
    node->identifier               = new AstIdentifier();
    node->pos                      = lxr.current().src_pos;
    node->identifier->parent       = node;
    node->identifier->symbol_index = sym->symbol_index;
    node->identifier->pos          = lxr.current().src_pos;


    if(lxr.current() == TOKEN_VALUE_ASSIGNMENT) {

        const size_t   curr_pos = lxr.current().src_pos;
        const uint32_t line     = lxr.current().line;

        lxr.advance(1);
        node->init_value = parse_expression(parser, lxr, true);

        if(node->init_value == nullptr) {
            delete node;
            return nullptr;
        }

        const auto expr_type = (*node->init_value)->type;
        if(!VALID_SUBEXPRESSION(expr_type)) {
            lxr.raise_error("Invalid subexpression.", curr_pos, line);
            delete node;
            return nullptr;
        }

        (*node->init_value)->parent = node;
        return node;
    }

    if(sym->flags & SYM_GLOBAL) {
        sym->type.flags &= ~TYPE_UNINITIALIZED;
    }

    sym->type.flags |= TYPE_DEFAULT_INIT;
    return node;
}


tak::AstNode*
tak::parse_inferred_decl(Symbol* var, Parser& parser, Lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_VALUE_ASSIGNMENT, "Expected '='.");
    parser_assert(var->type.flags & TYPE_INFERRED, "Passed symbol does not have inferred flag set.");


    const size_t   curr_pos = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;

    auto* node       = new AstVardecl();
    node->identifier = new AstIdentifier();
    node->pos        = var->src_pos;

    node->identifier->pos          = var->src_pos;
    node->identifier->parent       = node;
    node->identifier->symbol_index = var->symbol_index;

    bool state = false;
    defer_if(!state, [&] {
       delete node;
    });


    lxr.advance(1);
    auto* subexpr = parse_expression(parser, lxr, true);
    if(subexpr == nullptr) {
        return nullptr;
    }

    if(!VALID_SUBEXPRESSION(subexpr->type)) {
        lxr.raise_error(fmt("Invalid subexpression being assigned to variable \"{}\".", var->name), curr_pos, line);
        return nullptr;
    }

    subexpr->parent  = node;
    node->init_value = subexpr;
    var->type.name   = VAR_NONE;

    state = true;
    return node;
}


tak::AstNode*
tak::parse_decl(Parser& parser, Lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_IDENTIFIER, "Expected identifier.");

    const auto     name      = parser.namespace_as_string() + std::string(lxr.current().value);
    const size_t   src_pos   = lxr.current().src_pos;
    const uint32_t line      = lxr.current().line;
    uint64_t       typeflags = TYPE_FLAGS_NONE;
    uint32_t       symflags  = SYM_FLAGS_NONE;
    uint32_t       replace   = INVALID_SYMBOL_INDEX;


    lxr.advance(1);
    if(lxr.current() == TOKEN_CONST_TYPE_ASSIGNMENT) {
        typeflags |= TYPE_CONSTANT;
    }

    else if(lxr.current() != TOKEN_TYPE_ASSIGNMENT) {
        lxr.raise_error("Expected type assignment.");
        return nullptr;
    }

    if(parser.namespace_exists(std::string(lxr.current().value))) {
        lxr.raise_error("Confusion: variable has the same name as a namespace it is declared within.");
        return nullptr;
    }

    if(parser.scope_stack_.size() <= 1) {
        symflags |= SYM_GLOBAL;
    }


    if(parser.scoped_symbol_exists_at_current_scope(name)) {
        const uint32_t index = parser.lookup_scoped_symbol(name);
        auto* sym            = parser.lookup_unique_symbol(index);
        if(sym->flags & SYM_PLACEHOLDER) {
            sym->flags      &= ~SYM_PLACEHOLDER;
            replace          = index;
            sym->flags       = symflags;
            sym->src_pos     = src_pos;
            sym->line_number = line;
        } else {
            lxr.raise_error("Redeclaration: symbol already exists at this scope", src_pos, line);
            return nullptr;
        }
    }


    // Inferred type
    lxr.advance(1);
    if(lxr.current() == TOKEN_VALUE_ASSIGNMENT)
    {
        Symbol* inferred_ptr = nullptr;
        if(replace != INVALID_SYMBOL_INDEX) {
            inferred_ptr              = parser.lookup_unique_symbol(replace);
            inferred_ptr->type.kind   = TYPE_KIND_NONE;
            inferred_ptr->type.flags  = typeflags | TYPE_INFERRED | TYPE_UNINITIALIZED;
        } else {
            inferred_ptr = parser.create_symbol(name, src_pos, line, TYPE_KIND_NONE, typeflags | TYPE_INFERRED | TYPE_UNINITIALIZED);
        }

        assert(inferred_ptr != nullptr);
        inferred_ptr->flags = symflags;
        return parse_inferred_decl(inferred_ptr, parser, lxr);
    }


    if(lxr.current().kind != KIND_TYPE_IDENTIFIER && !TOKEN_IDENT_START(lxr.current().type)) {
        lxr.raise_error("Expected type identifier here.");
        return nullptr;
    }


    // Procedure (maybe pointer)
    if(lxr.current() == TOKEN_KW_PROC)
    {
        Symbol* proc_ptr = nullptr;
        if(replace != INVALID_SYMBOL_INDEX) {
            proc_ptr              = parser.lookup_unique_symbol(replace);
            proc_ptr->type.kind   = TYPE_KIND_PROCEDURE;
            proc_ptr->type.flags  = typeflags;
        } else {
            proc_ptr = parser.create_symbol(name, src_pos, line, TYPE_KIND_PROCEDURE, typeflags);
        }

        assert(proc_ptr != nullptr);
        proc_ptr->flags = symflags;

        if(lxr.peek(1) == TOKEN_BITWISE_XOR_OR_PTR) {
            proc_ptr->type.flags |= TYPE_POINTER | TYPE_UNINITIALIZED;
            return parse_proc_ptr(proc_ptr, parser, lxr);
        }

        return parse_procdecl(proc_ptr, parser, lxr);
    }


    // Struct or type alias decl
    if(TOKEN_IDENT_START(lxr.current().type))
    {
        Symbol* utype_ptr = nullptr;
        if(replace != INVALID_SYMBOL_INDEX) {
            utype_ptr              = parser.lookup_unique_symbol(replace);
            utype_ptr->type.kind   = TYPE_KIND_VARIABLE;
            utype_ptr->type.flags  = typeflags | TYPE_UNINITIALIZED;
        } else {
            utype_ptr = parser.create_symbol(name, src_pos, line, TYPE_KIND_VARIABLE, typeflags | TYPE_UNINITIALIZED);
        }

        assert(utype_ptr != nullptr);
        utype_ptr->flags = symflags;
        return parse_usertype_decl(utype_ptr, parser, lxr);
    }


    // Primitive type
    Symbol* var_ptr = nullptr;
    if(replace != INVALID_SYMBOL_INDEX) {
        var_ptr              = parser.lookup_unique_symbol(replace);
        var_ptr->type.kind   = TYPE_KIND_VARIABLE;
        var_ptr->type.flags  = typeflags | TYPE_UNINITIALIZED;
    } else {
        var_ptr = parser.create_symbol(name, src_pos, line, TYPE_KIND_VARIABLE, typeflags | TYPE_UNINITIALIZED);
    }

    assert(var_ptr != nullptr);
    var_ptr->flags = symflags;
    return parse_vardecl(var_ptr, parser, lxr);
}
