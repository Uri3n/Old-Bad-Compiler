//
// Created by Diago on 2024-07-09.
//

#include <parser.hpp>


std::optional<uint32_t>
parse_array_size(lexer& lxr) {

    PARSER_ASSERT(lxr.current() == LSQUARE_BRACKET, "Expected begin of [] (array specifier)");

    uint32_t len = 0;
    lxr.advance(1);


    if(lxr.current() == INTEGER_LITERAL) {

        const auto istr = std::string(lxr.current().value);

        try {
            len = std::stoul(istr);
        } catch(const std::out_of_range& _) {
            lxr.raise_error("Array size is too large.");
            return std::nullopt;
        } catch(...) {
            lxr.raise_error("Array size must be a valid non-negative integer literal.");
            return std::nullopt;
        }

        if(len == 0) {
            lxr.raise_error("Array length cannot be 0.");
            return std::nullopt;
        }

        lxr.advance(1);
    }


    if(lxr.current() != RSQUARE_BRACKET) {
        lxr.raise_error("Expected closing square bracket.");
        return std::nullopt;
    }

    lxr.advance(1);
    return len;
}


std::optional<type_data>
parse_type(parser& parser, lexer& lxr) {

    PARSER_ASSERT(lxr.current().kind == TYPE_IDENTIFIER || lxr.current() == IDENTIFIER, "Expected type.");
    type_data data;


    //
    // Determine if variable, struct, or proc.
    //

    if(lxr.current() == TOKEN_KW_PROC) {
        data.sym_type = SYM_PROCEDURE;
        data.name     = std::monostate();
    }

    else if(lxr.current().kind == IDENTIFIER) {
        if(!parser.type_exists(std::string(lxr.current().value))) {
            lxr.raise_error("Invalid type specifier.");
            return std::nullopt;
        }

        data.sym_type = SYM_STRUCT;
        data.name     = std::string(lxr.current().value);
    }

    else {
        const var_t _var_t = token_to_var_t(lxr.current().type);
        if(_var_t == VAR_NONE) {
            lxr.raise_error("Invalid type specifier.");
            return std::nullopt;
        }

        data.name     = _var_t;
        data.sym_type = SYM_VARIABLE;
    }


    //
    // Get pointer depth, array size (if exists)
    //

    lxr.advance(1);
    if(lxr.current() == BITWISE_XOR_OR_PTR) {
        data.flags |= SYM_IS_POINTER;
        while(lxr.current() == BITWISE_XOR_OR_PTR) {
            data.pointer_depth++;
            lxr.advance(1);
        }
    }

    if(lxr.current() == LSQUARE_BRACKET) {
        data.flags |= SYM_IS_ARRAY;
        if(const auto arr_size = parse_array_size(lxr)) {
            data.array_length = *arr_size;
        } else {
            return std::nullopt;
        }
    }


    //
    // If the type isn't a proc, we can simply return here.
    //

    if(data.sym_type != SYM_PROCEDURE) {
        return data;
    }

    if(lxr.current() != LPAREN) {
        lxr.raise_error("Expected beginning of parameter type list.");
        return std::nullopt;
    }


    //
    // Parse procedure parameter list.
    //

    lxr.advance(1);
    data.parameters = std::make_shared<std::vector<type_data>>();

    while(lxr.current() != RPAREN) {

        if(lxr.current().kind != TYPE_IDENTIFIER && lxr.current() != IDENTIFIER) {
            lxr.raise_error("Expected type identifier.");
            return std::nullopt;
        }

        if(const auto param_data = parse_type(parser, lxr)) {
            data.parameters->emplace_back(*param_data);
        } else {
            return std::nullopt;
        }

        if(lxr.current() == COMMA) {
            lxr.advance(1);
        }
    }


    //
    // Get return type. If VOID, we can just leave data.return_type as nullptr and return.
    //

    if(lxr.peek(1) != ARROW || (lxr.peek(2).kind != TYPE_IDENTIFIER && lxr.peek(2) != IDENTIFIER)) {
        lxr.raise_error("Expected procedure return type after parameter list. Example: -> i32");
        return std::nullopt;
    }

    lxr.advance(2);
    if(lxr.current() == TOKEN_KW_VOID) {
        lxr.advance(1);
        return data;
    }


    data.return_type = std::make_shared<type_data>();
    if(const auto ret_type = parse_type(parser, lxr)) {
        *data.return_type = *ret_type; // expensive copy
    } else {
        return std::nullopt;
    }

    return data;
}


ast_node*
parse_proc_ptr(symbol* proc, parser& parser, lexer& lxr) {

    PARSER_ASSERT(lxr.current() == TOKEN_KW_PROC, "Expected \"proc\" keyword.");
    PARSER_ASSERT(lxr.peek(1) == BITWISE_XOR_OR_PTR, "Expected next token to be pointy fella (^).");
    PARSER_ASSERT(proc != nullptr, "Null symbol pointer.");

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
    bool state       = false;
    auto _           = defer([&]{ if(!state){ delete node; } });

    node->identifier->parent       = node;
    node->identifier->symbol_index = proc->symbol_index;


    lxr.advance(1);
    if(lxr.current() == VALUE_ASSIGNMENT) {

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


    proc->type.flags |= SYM_DEFAULT_INITIALIZED;
    state = true;
    return node;
}


ast_vardecl*
parse_parameterized_vardecl(parser& parser, lexer& lxr) {

    PARSER_ASSERT(lxr.current() == IDENTIFIER, "Expected variable identifier.");

    const auto     name      = std::string(lxr.current().value);
    const size_t   src_pos   = lxr.current().src_pos;
    const uint32_t line      = lxr.current().line;
    uint16_t       flags     = SYM_IS_PROCARG;
    uint16_t       ptr_depth = 0;
    var_t          var_type  = VAR_NONE;


    lxr.advance(1);
    if(lxr.current() == CONST_TYPE_ASSIGNMENT) {
        flags |= SYM_IS_CONSTANT;
    }

    else if(lxr.current() != TYPE_ASSIGNMENT) {
        lxr.raise_error("Expected type assignment here. Got this instead.");
        return nullptr;
    }

    lxr.advance(1);
    if(lxr.current().kind != TYPE_IDENTIFIER && lxr.current() != IDENTIFIER) {
        lxr.raise_error("Expected type identifier.");
        return nullptr;
    }

    if(lxr.current() == TOKEN_KW_VOID) {
        lxr.raise_error("Void can only be used as a procedure return type.");
        return nullptr;
    }

    if(lxr.current() == TOKEN_KW_PROC) {
        lxr.raise_error("Procedures cannot be used as procedure parameters.");
        return nullptr;
    }

    var_type = token_to_var_t(lxr.current().type);
    if(var_type == VAR_NONE) {
        lxr.raise_error("Unrecognized type identifier.");
        return nullptr;
    }


    lxr.advance(1);
    if(lxr.current() == BITWISE_XOR_OR_PTR) {
        flags |= SYM_IS_POINTER;
        while(lxr.current() == BITWISE_XOR_OR_PTR) {
            ptr_depth++;
            lxr.advance(1);
        }
    }


    if(lxr.current() == LSQUARE_BRACKET) {
        lxr.raise_error("Static arrays cannot be used as parameters. Pass an array as a pointer instead.");
        return nullptr;
    }

    if(parser.scoped_symbol_exists_at_current_scope(name)) {   // a new scope should be pushed by parse_procdecl
        lxr.raise_error("Symbol already exists within this scope.");
        return nullptr;
    }


    auto* var_ptr = parser.create_symbol(name, src_pos, line, SYM_VARIABLE, flags);
    if(var_ptr == nullptr) {
        return nullptr;
    }


    var_ptr->type.array_length  = 0;
    var_ptr->type.pointer_depth = ptr_depth;
    var_ptr->type.name          = var_type;

    auto* vardecl        = new ast_vardecl();
    vardecl->identifier  = new ast_identifier();

    vardecl->identifier->parent       = vardecl;
    vardecl->identifier->symbol_index = var_ptr->symbol_index;

    return vardecl;
}


ast_node*
parse_procdecl(symbol* proc, parser& parser, lexer& lxr) {

    PARSER_ASSERT(lxr.current() == TOKEN_KW_PROC, "Expected proc type identifier.");


    if(!(proc->type.flags & SYM_IS_GLOBAL)) {
        lxr.raise_error("Declaration of procedure at non-global scope.");
        return nullptr;
    }

    if(!(proc->type.flags & SYM_IS_CONSTANT)) {
        lxr.raise_error("Procedures must be declared as constant. This one was declared using ':'.");
        return nullptr;
    }

    lxr.advance(1);
    if(lxr.current() != LPAREN) {
        lxr.raise_error("Expected parameter list here.");
        return nullptr;
    }


    parser.push_scope();

    auto* node         = new ast_procdecl();
    node->identifier   = new ast_identifier();

    node->identifier->symbol_index = proc->symbol_index;
    node->identifier->parent       = node;

    bool state = false;
    auto _     = defer([&] {
        if(!state) { delete node; }
        parser.pop_scope();
    });


    //
    // Get procedure parameters (if any)
    //

    lxr.advance(1);
    while(lxr.current() != RPAREN) {

        if(lxr.current() != IDENTIFIER) {
            lxr.raise_error("Expected procedure parameter.");
            return nullptr;
        }

        auto* param = parse_parameterized_vardecl(parser, lxr);
        if(param == nullptr) {
            return nullptr;
        }

        param->parent = node;
        node->parameters.emplace_back(param);

        if(lxr.current() == COMMA) {
            lxr.advance(1);
        }

    }


    //
    // Get return type (remember we allow "void" here).
    //

    lxr.advance(1);
    if(lxr.current() != ARROW || (lxr.peek(1).kind != TYPE_IDENTIFIER && lxr.peek(1) != IDENTIFIER)) {
        lxr.raise_error("Expected procedure return type after parameter list.");
        return nullptr;
    }

    lxr.advance(1);
    if(lxr.current() == TOKEN_KW_VOID) {
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

    lxr.advance(1);
    if(lxr.current() != LBRACE) {
        lxr.raise_error("Expected start of procedure body here.");
        return nullptr;
    }

    //
    // Now we parse the rest of the procedure body. Just keep calling parse_expression
    // and checking if the returned AST node can be represented inside of a procedure body.
    //

    lxr.advance(1);
    while(lxr.current() != RBRACE) {

        size_t   curr_pos = lxr.current().src_pos;
        uint32_t line     = lxr.current().line;

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

    PARSER_ASSERT(lxr.current().kind == TYPE_IDENTIFIER, "Expected type identifier.");

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

    node->identifier->symbol_index = var->symbol_index;
    node->identifier->parent       = node;

    auto _ = defer([&]{ if(!state) { delete node; } });


    if(lxr.current() == VALUE_ASSIGNMENT) {

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


    var->type.flags |= SYM_DEFAULT_INITIALIZED;
    state = true;
    return node;
}


ast_node*
parse_structdecl(symbol* _struct, parser& parser, lexer& lxr) {

}


ast_node*
parse_decl(parser& parser, lexer& lxr) {

    PARSER_ASSERT(lxr.current() == IDENTIFIER, "Expected identifier.");

    const auto     name     = std::string(lxr.current().value);
    const size_t   src_pos  = lxr.current().src_pos;
    const uint32_t line     = lxr.current().line;
    uint16_t       flags    = SYM_FLAGS_NONE;
    sym_t          type     = SYM_VARIABLE;


    lxr.advance(1);
    if(lxr.current() == CONST_TYPE_ASSIGNMENT) {
        flags |= SYM_IS_CONSTANT;
    }

    else if(lxr.current() != TYPE_ASSIGNMENT) {
        lxr.raise_error("Expected type assignment.");
        return nullptr;
    }

    if(parser.scope_stack.size() <= 1) {
        flags |= SYM_IS_GLOBAL;
    }


    //
    // Check for redeclarations.
    //

    if(parser.scoped_symbol_exists_at_current_scope(name)) {
        lxr.raise_error("Symbol redeclaration, this already exists at the current scope.", src_pos, line);
        return nullptr;
    }


    lxr.advance(1);
    if(lxr.current().kind != TYPE_IDENTIFIER && lxr.current() != IDENTIFIER) {
        lxr.raise_error("Expected type identifier here.");
        return nullptr;
    }

    if(lxr.current() == TOKEN_KW_VOID) { // This is technically a type ident, but only used for returns.
        lxr.raise_error("Type identifier \"void\" can only be used as a return type.");
        return nullptr;
    }


    //
    // Parse if procedure
    //

    if(lxr.current() == TOKEN_KW_PROC) {

        type           = SYM_PROCEDURE;
        auto* proc_ptr = parser.create_symbol(name, src_pos, line, type, flags);
        if(proc_ptr == nullptr) {
            return nullptr;
        }

        if(lxr.peek(1) == BITWISE_XOR_OR_PTR) { // function pointer
            proc_ptr->type.flags |= SYM_IS_POINTER;
            return parse_proc_ptr(proc_ptr, parser, lxr);
        }

        return parse_procdecl(proc_ptr, parser, lxr);
    }


    //
    // Parse if struct
    //

    if(lxr.current() == IDENTIFIER) {

        type             = SYM_STRUCT;
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