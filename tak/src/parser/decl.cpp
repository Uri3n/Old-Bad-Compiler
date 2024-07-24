//
// Created by Diago on 2024-07-09.
//

#include <parser.hpp>


ast_node*
parse_proc_ptr(symbol* proc, parser& parser, lexer& lxr) {

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

    auto* node       = new ast_vardecl();
    node->identifier = new ast_identifier();
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

    proc->type.flags |= TYPE_DEFAULT_INITIALIZED;
    state = true;
    return node;
}


ast_vardecl*
parse_parameterized_vardecl(parser& parser, lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_IDENTIFIER, "Expected variable identifier.");

    const auto     name      = parser.namespace_as_string() + std::string(lxr.current().value);
    const size_t   src_pos   = lxr.current().src_pos;
    const uint32_t line      = lxr.current().line;
    uint16_t       flags     = TYPE_IS_PROCARG;


    if(parser.namespace_exists(std::string(lxr.current().value))) {
        lxr.raise_error("Confusion: variable has the same name as a namespace it is declared in.");
        return nullptr;
    }

    if(parser.scoped_symbol_exists_at_current_scope(name)) {   // a new scope should be pushed by parse_procdecl
        lxr.raise_error("Symbol already exists within this scope.");
        return nullptr;
    }

    lxr.advance(1);
    if(lxr.current() == TOKEN_CONST_TYPE_ASSIGNMENT) {
        flags |= TYPE_IS_CONSTANT;
    }

    else if(lxr.current() != TOKEN_TYPE_ASSIGNMENT) {
        lxr.raise_error("Expected type assignment.");
        return nullptr;
    }

    lxr.advance(1);
    if(lxr.current().kind != KIND_TYPE_IDENTIFIER && lxr.current() != TOKEN_IDENTIFIER) {
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

    if(_type_data->sym_type == TYPE_KIND_PROCEDURE && _type_data->pointer_depth < 1) {
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

    const auto* var_ptr = parser.create_symbol(name, src_pos, line, _type_data->sym_type, flags, _type_data);
    if(var_ptr == nullptr) {
        return nullptr;
    }

    auto* vardecl        = new ast_vardecl();
    vardecl->identifier  = new ast_identifier();
    vardecl->pos         = src_pos;

    vardecl->identifier->parent       = vardecl;
    vardecl->identifier->symbol_index = var_ptr->symbol_index;
    vardecl->identifier->pos          = src_pos;

    return vardecl;
}


ast_node*
parse_procdecl(symbol* proc, parser& parser, lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_KW_PROC, "Expected proc type identifier.");


    if(!(proc->type.flags & TYPE_IS_GLOBAL)) {
        lxr.raise_error("Declaration of procedure at non-global scope.");
        return nullptr;
    }

    if(!(proc->type.flags & TYPE_IS_CONSTANT)) {
        lxr.raise_error("Procedures must be declared as constant. This one was declared using ':'.");
        return nullptr;
    }

    lxr.advance(1);
    if(lxr.current() != TOKEN_LPAREN) {
        lxr.raise_error("Expected parameter list here.");
        return nullptr;
    }


    //
    // Create AST node
    //

    parser.push_scope();

    auto* node         = new ast_procdecl();
    node->identifier   = new ast_identifier();
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
    // Get return type (remember we allow "void" here).
    //

    lxr.advance(1);
    if(lxr.current() != TOKEN_ARROW || (lxr.peek(1).kind != KIND_TYPE_IDENTIFIER && lxr.peek(1) != TOKEN_IDENTIFIER)) {
        lxr.raise_error("Expected procedure return type after parameter list.");
        return nullptr;
    }

    lxr.advance(1);
    if(lxr.current() == TOKEN_KW_VOID && lxr.peek(1) != TOKEN_BITWISE_XOR_OR_PTR) {
        lxr.advance(1);
    } else if(const auto _type_data = parse_type(parser, lxr)) {
        proc->type.return_type  = std::make_shared<type_data>();
        *proc->type.return_type = *_type_data;
    } else {
        return nullptr;
    }


    //
    // Now we should fill out the parameter types in the symbol table for easy lookup later.
    //

    if(!node->parameters.empty()) {
        proc->type.parameters = std::make_shared<std::vector<type_data>>();
    }

    for(const auto& param : node->parameters) {
        const auto* symbol_ptr = parser.lookup_unique_symbol(param->identifier->symbol_index);
        if(symbol_ptr == nullptr) {
            return nullptr;
        }

        proc->type.parameters->emplace_back(symbol_ptr->type);
    }


    //
    // In the future we should just be leaving this as a definition.
    //

    if(lxr.current() != TOKEN_LBRACE) {
        lxr.raise_error("Expected start of procedure body here.");
        return nullptr;
    }

    //
    // Now we parse the rest of the procedure body. Just keep calling parse_expression
    // and checking if the returned AST node can be represented inside of a procedure body.
    //

    lxr.advance(1);
    while(lxr.current() != TOKEN_RBRACE) {

        const size_t   curr_pos = lxr.current().src_pos;
        const uint32_t line     = lxr.current().line;

        auto* expr = parse_expression(parser, lxr, false);
        if(expr == nullptr) {
            return nullptr;
        }

        if(expr->type == NODE_STRUCT_DEFINITION || expr->type == NODE_PROCDECL || expr->type == NODE_ENUM_DEFINITION) {
            lxr.raise_error("Illegal expression inside of procedure body.", curr_pos, line);
            delete expr;
            return nullptr;
        }

        expr->parent = node;
        node->body.emplace_back(expr);
    }


    lxr.advance(1);
    state = true;
    return node;
}


ast_node*
parse_vardecl(symbol* var, parser& parser, lexer& lxr) {

    parser_assert(lxr.current().kind == KIND_TYPE_IDENTIFIER, "Expected type identifier.");

    const auto type_data = parse_type(parser, lxr);
    if(!type_data) {
        return nullptr;
    }

    const uint16_t temp = var->type.flags;
    var->type           = *type_data;
    var->type.flags    |= temp;


    //
    // Generate AST node
    //

    bool  state       = false;
    auto* node        = new ast_vardecl();
    node->identifier  = new ast_identifier();
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


    var->type.flags |= TYPE_DEFAULT_INITIALIZED;
    state = true;
    return node;
}


ast_node*
parse_structdecl(symbol* _struct, parser& parser, lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_IDENTIFIER, "Expected struct type name.");

    if(const auto type = parse_type(parser, lxr)) {
        const uint16_t temp  = _struct->type.flags;
        _struct->type        = *type;
        _struct->type.flags |= temp;
    } else {
        return nullptr;
    }


    auto* node                     = new ast_vardecl();
    node->identifier               = new ast_identifier();
    node->pos                      = lxr.current().src_pos;
    node->identifier->parent       = node;
    node->identifier->symbol_index = _struct->symbol_index;
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
            lxr.raise_error("Invalid subexpression being assigned to struct.", curr_pos, line);
            delete node;
            return nullptr;
        }

        (*node->init_value)->parent = node;
        return node;
    }

    _struct->type.flags |= TYPE_DEFAULT_INITIALIZED;
    return node;
}


ast_node*
parse_decl(parser& parser, lexer& lxr) {

    parser_assert(lxr.current() == TOKEN_IDENTIFIER, "Expected identifier.");


    const auto     name     = parser.namespace_as_string() + std::string(lxr.current().value);
    const size_t   src_pos  = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;
    uint16_t       flags    = TYPE_FLAGS_NONE;
    type_kind_t          type     = TYPE_KIND_VARIABLE;


    lxr.advance(1);
    if(lxr.current() == TOKEN_CONST_TYPE_ASSIGNMENT) {
        flags |= TYPE_IS_CONSTANT;
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
        flags |= TYPE_IS_GLOBAL;
    }


    //
    // Check for redeclarations.
    //

    if(parser.scoped_symbol_exists_at_current_scope(name)) {
        lxr.raise_error("Symbol redeclaration, this already exists at the current scope.", src_pos, line);
        return nullptr;
    }

    lxr.advance(1);
    if(lxr.current().kind != KIND_TYPE_IDENTIFIER && lxr.current() != TOKEN_IDENTIFIER) {
        lxr.raise_error("Expected type identifier here.");
        return nullptr;
    }


    //
    // Parse if procedure
    //

    if(lxr.current() == TOKEN_KW_PROC) {

        type           = TYPE_KIND_PROCEDURE;
        auto* proc_ptr = parser.create_symbol(name, src_pos, line, type, flags);
        if(proc_ptr == nullptr) {
            return nullptr;
        }

        if(lxr.peek(1) == TOKEN_BITWISE_XOR_OR_PTR) { // function pointer
            proc_ptr->type.flags |= TYPE_IS_POINTER;
            return parse_proc_ptr(proc_ptr, parser, lxr);
        }

        return parse_procdecl(proc_ptr, parser, lxr);
    }


    //
    // Parse if struct
    //

    if(lxr.current() == TOKEN_IDENTIFIER) {

        type             = TYPE_KIND_STRUCT;
        auto* struct_ptr = parser.create_symbol(name, src_pos, line, type, flags);

        if(struct_ptr == nullptr) {
            return nullptr;
        }

        return parse_structdecl(struct_ptr, parser, lxr);
    }


    //
    // Parse if variable
    //

    auto* var_ptr = parser.create_symbol(name, src_pos, line, type, flags);
    if(var_ptr == nullptr) {
        return nullptr;
    }

    return parse_vardecl(var_ptr, parser, lxr);
}
