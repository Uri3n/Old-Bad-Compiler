//
// Created by Diago on 2024-07-08.
//

#include <parser.hpp>


/////////////////////////////////////////////////////////////////////
// I need an easy way to view the generated AST...
//
// x = (3 - 10) + 1000;
// should display as something like:
//
// assign
//   |- identifier x
//   |- add +
//        |- sub -
//             |- lit 3
//             |- lit 10
//        |- lit 1000
//


static void
display_node_vardecl(ast_node* node, std::string& node_title, const uint32_t depth, parser& _) {

    node_title += "Variable Declaration";
    const auto* vardecl = dynamic_cast<ast_vardecl*>(node);

    if(vardecl == nullptr) {
        node_title += " !! INVALID NODE TYPE";
        print("{}", node_title);
        return;
    }


    print("{}", node_title);
    display_node_data(vardecl->identifier, depth + 1, _);
    if(vardecl->init_value.has_value()) {
        display_node_data(*(vardecl->init_value), depth + 1, _);
    }
}


static void
display_node_procdecl(ast_node* node, std::string& node_title, uint32_t depth, parser& _) {

    const auto* procdecl = dynamic_cast<ast_procdecl*>(node);

    if(procdecl == nullptr) {
        node_title += " !! INVALID NODE TYPE";
        print("{}", node_title);
        return;
    }


    print("{}Procedure Declaration", node_title);
    display_node_data(procdecl->identifier, depth + 1, _);

    if(!procdecl->parameters.empty() || !procdecl->body.empty()) {
        node_title.insert(0, "     ");
        if(!depth) {
            node_title += "|- ";
        }

        ++depth;
    }


    //
    // NOTE: Parameters and Procedure Body are fake nodes.
    // I still print them here as nodes to make it easy to interpret the AST but really
    // we store both of these things as std::vector<ast_node*> inside of the procdecl node.
    //

    if(!procdecl->parameters.empty()) {
        print("{}Parameters", node_title);
        for(ast_node* param : procdecl->parameters) {
            display_node_data(param, depth + 1, _);
        }
    }

    if(!procdecl->body.empty()) {
        print("{}Procedure Body", node_title);
        for(ast_node* child : procdecl->body) {
            display_node_data(child, depth + 1, _);
        }
    }
}


static void
display_node_assign(ast_node* node, std::string& node_title, const uint32_t depth, parser& _) {

    node_title += "Assign";
    const auto* assign = dynamic_cast<ast_assign*>(node);

    if(assign == nullptr) {
        node_title += " !! INVALID NODE TYPE";
        print("{}", node_title);
        return;
    }

    print("{}", node_title);
    display_node_data(assign->assigned, depth + 1, _);
    display_node_data(assign->expression, depth + 1, _);
}


static void
display_node_binexpr(ast_node* node, std::string& node_title, const uint32_t depth, parser& _) {

    const auto* binexpr = dynamic_cast<ast_binexpr*>(node);

    if(binexpr == nullptr) {
        node_title += "(Binary Expression) !! INVALID NODE TYPE";
        print("{}", node_title);
        return;
    }

    node_title += lexer_token_type_to_string(binexpr->_operator);
    print("{} (Binary Expression)", node_title);

    display_node_data(binexpr->left_op, depth + 1, _);
    display_node_data(binexpr->right_op, depth + 1, _);
}


static void
display_node_unaryexpr(ast_node* node, std::string& node_title, const uint32_t depth, parser& _) {

    const auto* unaryexpr = dynamic_cast<ast_unaryexpr*>(node);

    if(unaryexpr == nullptr) {
        node_title += "(Unary Expression) !! INVALID NODE TYPE";
        print("{}", node_title);
        return;
    }

    node_title += lexer_token_type_to_string(unaryexpr->_operator);
    print("{} (Unary Expression)", node_title);

    display_node_data(unaryexpr->operand, depth + 1, _);
}


static void
display_node_identifier(ast_node* node, std::string& node_title, parser& parser) {

    const auto* ident = dynamic_cast<ast_identifier*>(node);

    if(ident == nullptr) {
        node_title += "(Ident) !! INVALID NODE TYPE";
        print("{}", node_title);
        return;
    }

    const symbol* sym_ptr = parser.lookup_unique_symbol(ident->symbol_index);
    if(sym_ptr == nullptr) {
        node_title += fmt("(Ident) (Sym Index {}) !! NOT IN SYMBOL TABLE", ident->symbol_index);
        print("{}", node_title);
        return;
    }


    std::string sym_t_str;
    if(sym_ptr->type.sym_type == SYM_PROCEDURE) {
        sym_t_str = "Procedure";
    } else if(sym_ptr->type.sym_type == SYM_VARIABLE) {
        sym_t_str = "Variable";
    } else {
        sym_t_str = "Struct";
    }


    node_title += fmt(
        "{} ({}) (Sym Index {})",
        sym_ptr->name,
        sym_t_str,
        sym_ptr->symbol_index
    );

    print("{}", node_title);
}


static void
display_node_literal(ast_node* node, std::string& node_title) {

    const auto* lit = dynamic_cast<ast_singleton_literal*>(node);

    if(lit == nullptr) {
        node_title += "(Literal) !! INVALID NODE TYPE";
        print("{}", node_title);
        return;
    }

    node_title += fmt("{} ({})", lit->value, lexer_token_type_to_string(lit->literal_type));
    print("{}", node_title);
}


void
display_node_data(ast_node* node, const uint32_t depth, parser& parser) {

    const uint32_t num_spaces = (depth * 2) + (depth * 3);
    std::string    node_title;

    for(uint32_t i = 0; i < num_spaces; ++i)
        node_title += ' ';

    if(depth) {
        node_title += "|- ";
    }


    switch(node->type) {
        case NODE_NONE:
            print("{}None", node_title);
            break;

        case NODE_VARDECL:
            display_node_vardecl(dynamic_cast<ast_vardecl*>(node), node_title, depth, parser);
            break;

        case NODE_PROCDECL:
            display_node_procdecl(dynamic_cast<ast_procdecl*>(node), node_title, depth, parser);
            break;

        case NODE_BINEXPR:
            display_node_binexpr(dynamic_cast<ast_binexpr*>(node), node_title, depth, parser);
            break;

        case NODE_UNARYEXPR:
            display_node_unaryexpr(dynamic_cast<ast_unaryexpr*>(node), node_title, depth, parser);
            break;

        case NODE_ASSIGN:
            display_node_assign(dynamic_cast<ast_assign*>(node), node_title, depth, parser);
            break;

        case NODE_IDENT:
            display_node_identifier(dynamic_cast<ast_identifier*>(node), node_title, parser);
            break;

        case NODE_SINGLETON_LITERAL:
            display_node_literal(dynamic_cast<ast_singleton_literal*>(node), node_title);
            break;

        /*
        case NODE_BRANCH:
        case NODE_IF:
        case NODE_ELSE:
        case NODE_FOR:
        case NODE_SWITCH:
        case NODE_CASE:
        case NODE_DEFAULT:
        case NODE_WHILE:
        case NODE_CALL:
        case NODE_BRK:
        case NODE_CONT:
        case NODE_RET:
        case NODE_BRACED_EXPRESSION:
        case NODE_STRUCT_DEFINITION:
        */

        default:
            print("{}{}", node_title, "No display impl for this node yet...");
            break;
    }
}


void
parser::dump_nodes() {
    for(const auto node : toplevel_decls) {
        display_node_data(node, 0, *this);
    }
}