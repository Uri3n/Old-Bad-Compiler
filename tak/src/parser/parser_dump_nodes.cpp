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
display_fake_node(const std::string& name, std::string& node_title, uint32_t& depth) {

    node_title.insert(0, "     ");
    if(!depth) {
        node_title += "|- ";
    }

    ++depth;
    print("{}{}", node_title, name);
}

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

    if(ident->member_name.has_value()) {
        node_title += fmt(" Accessing Member: {}", *ident->member_name);
    }

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

static void
display_node_call(ast_node* node, std::string& node_title, uint32_t depth, parser& parser) {

    const auto* call = dynamic_cast<ast_call*>(node);
    if(call == nullptr) {
        print("{} (Call) !! INVALID NODE TYPE", node_title);
        return;
    }


    print("{}Procedure Call", node_title);
    display_node_data(call->identifier, depth + 1, parser);

    if(!call->arguments.empty()) {
        display_fake_node("Arguments", node_title, depth);
    }

    for(ast_node* arg : call->arguments) {
        display_node_data(arg, depth + 1, parser);
    }
}

static void
display_node_brk(ast_node* node, const std::string& node_title) {

    const auto* brk = dynamic_cast<ast_brk*>(node);
    if(brk == nullptr) {
        print("{} (Break) !! INVALID NODE TYPE", node_title);
        return;
    }

    print("{}Break Statement", node_title);
}

static void
display_node_cont(ast_node* node, const std::string& node_title) {

    const auto* cont = dynamic_cast<ast_cont*>(node);
    if(cont == nullptr) {
        print("{} (Continue) !! INVALID NODE TYPE", node_title);
        return;
    }

    print("{}Continue Statement", node_title);
}

static void
display_node_ret(ast_node* node, const std::string& node_title, const uint32_t depth, parser& _) {

    const auto* ret = dynamic_cast<ast_ret*>(node);
    if(ret == nullptr) {
        print("{} (Return) !! INVALID NODE TYPE", node_title);
        return;
    }

    print("{}Return", node_title);
    if(ret->value.has_value()) {
        display_node_data(*ret->value, depth + 1, _);
    }
}

static void
display_node_structdef(ast_node* node, const std::string& node_title) {

    const auto* _struct = dynamic_cast<ast_structdef*>(node);
    if(_struct == nullptr) {
        print("{} (Struct Definition) !! INVALID NODE TYPE", node_title);
        return;
    }

    print("{}{} (Struct Definition)", node_title, _struct->name);
}

static void
display_node_braced_expression(ast_node* node, const std::string& node_title, const uint32_t depth, parser& _) {

    const auto* expr = dynamic_cast<ast_braced_expression*>(node);
    if(expr == nullptr) {
        print("{} (Braced Expression) !! INVALID NODE TYPE", node_title);
        return;
    }

    print("{}Braced Expression", node_title);
    for(ast_node* member : expr->members) {
        display_node_data(member, depth + 1, _);
    }
}

static void
display_node_branch(ast_node* node, const std::string& node_title, const uint32_t depth, parser& _) {

    const auto* branch = dynamic_cast<ast_branch*>(node);
    if(branch == nullptr) {
        print("{} (Branch) !! INVALID NODE TYPE", node_title);
        return;
    }

    print("{}Branch", node_title);
    for(ast_node* if_stmt : branch->conditions) {
        display_node_data(if_stmt, depth + 1, _);
    }

    if(branch->_else.has_value()) {
        display_node_data(*branch->_else, depth + 1, _);
    }
}

static void
display_node_if(ast_node* node, std::string& node_title, uint32_t depth, parser& _) {

    const auto* if_stmt = dynamic_cast<ast_if*>(node);
    if(if_stmt == nullptr) {
        print("{} (If) !! INVALID NODE TYPE", node_title);
        return;
    }

    print("{}If", node_title);
    display_node_data(if_stmt->condition, depth + 1, _);
    display_fake_node("Body", node_title, depth);

    for(ast_node* expr :  if_stmt->body) {
        display_node_data(expr, depth + 1, _);
    }
}

static void
display_node_while(ast_node* node, std::string& node_title, uint32_t depth, parser& _) {

    const auto* _while = dynamic_cast<ast_while*>(node);
    if(_while == nullptr) {
        print("{} (While) !! INVALID NODE TYPE", node_title);
        return;
    }

    print("{}While", node_title);
    display_node_data(_while->condition, depth + 1, _);
    display_fake_node("Body", node_title, depth);

    for(ast_node* expr : _while->body) {
        display_node_data(expr, depth + 1, _);
    }
}

static void
display_node_else(ast_node* node, std::string& node_title, uint32_t depth, parser& _) {

    const auto* else_stmt = dynamic_cast<ast_else*>(node);
    if(else_stmt == nullptr) {
        print("{} (Else) !! INVALID NODE TYPE", node_title);
        return;
    }

    print("{}Else", node_title);
    display_fake_node("Body", node_title, depth);

    for(ast_node* expr :  else_stmt->body) {
        display_node_data(expr, depth + 1, _);
    }
}

static void
display_node_case(ast_node* node, std::string& node_title, uint32_t depth, parser& _) {

    const auto* _case = dynamic_cast<ast_case*>(node);
    if(_case == nullptr) {
        print("{} (Default) !! INVALID NODE TYPE", node_title);
        return;
    }

    print("{}Case {} (Fallthrough={})", node_title, _case->value->value, _case->fallthrough ? "True" : "False");
    display_fake_node("Body", node_title, depth);

    for(ast_node* expr : _case->body) {
        display_node_data(expr, depth + 1, _);
    }
}

static void
display_node_default(ast_node* node, std::string& node_title, uint32_t depth, parser& _) {

    const auto* _default = dynamic_cast<ast_default*>(node);
    if(_default == nullptr) {
        print("{} (Default) !! INVALID NODE TYPE", node_title);
        return;
    }

    print("{}Default", node_title);
    display_fake_node("Body", node_title, depth);

    for(ast_node* expr : _default->body) {
        display_node_data(expr, depth + 1, _);
    }
}

static void
display_node_switch(ast_node* node, const std::string& node_title, const uint32_t depth, parser& _) {

    const auto* _switch = dynamic_cast<ast_switch*>(node);
    if(_switch == nullptr) {
        print("{} (Switch) !! INVALID NODE TYPE", node_title);
        return;
    }

    print("{}Switch", node_title);

    display_node_data(_switch->target, depth + 1, _);
    for(ast_node* _case : _switch->cases) {
        display_node_data(_case, depth + 1, _);
    }

    display_node_data(_switch->_default, depth + 1, _);
}

static void
display_node_for(ast_node* node, std::string& node_title, uint32_t depth, parser& _) {

    const auto* _for = dynamic_cast<ast_for*>(node);
    if(_for == nullptr) {
        print("{} (For) !! INVALID NODE TYPE", node_title);
        return;
    }

    print("{}For", node_title);

    uint32_t    tmp_depth = depth;
    std::string tmp_title = node_title;


    if(_for->init.has_value()) {
        display_fake_node("Initialization", tmp_title, tmp_depth);
        display_node_data(*_for->init, tmp_depth + 1, _);
    }

    if(_for->condition.has_value()) {
        tmp_title = node_title;
        tmp_depth = depth;
        display_fake_node("Condition", tmp_title, tmp_depth);
        display_node_data(*_for->condition, tmp_depth + 1, _);
    }

    if(_for->update.has_value()) {
        tmp_title = node_title;
        tmp_depth = depth;
        display_fake_node("Update", tmp_title, tmp_depth);
        display_node_data(*_for->update, tmp_depth + 1, _);
    }


    display_fake_node("Body", node_title, depth);
    for(ast_node* expr : _for->body) {
        display_node_data(expr, depth + 1, _);
    }
}

static void
display_node_subscript(ast_node* node, const std::string& node_title, const uint32_t depth, parser& _) {

    const auto* subscript = dynamic_cast<ast_subscript*>(node);
    if(subscript == nullptr) {
        print("{} (For) !! INVALID NODE TYPE", node_title);
        return;
    }

    print("{}Index Into (Subscript)", node_title);
    display_node_data(subscript->operand, depth + 1, _);
    display_node_data(subscript->value, depth + 1, _);
}

static void
display_node_namespacedecl(ast_node* node, const std::string& node_title, const uint32_t depth, parser& _) {

    const auto* _namespace = dynamic_cast<ast_namespacedecl*>(node);
    if(_namespace == nullptr) {
        print("{} (For) !! INVALID NODE TYPE", node_title);
        return;
    }

    print("{}{} (Namespace Decl)", node_title, _namespace->full_path);
    for(ast_node* expr : _namespace->children) {
        display_node_data(expr, depth + 1, _);
    }
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
        case NODE_VARDECL:            display_node_vardecl(node, node_title, depth, parser); break;
        case NODE_PROCDECL:           display_node_procdecl(node, node_title, depth, parser); break;
        case NODE_BINEXPR:            display_node_binexpr(node, node_title, depth, parser); break;
        case NODE_UNARYEXPR:          display_node_unaryexpr(node, node_title, depth, parser); break;
        case NODE_ASSIGN:             display_node_assign(node, node_title, depth, parser); break;
        case NODE_IDENT:              display_node_identifier(node, node_title, parser); break;
        case NODE_SINGLETON_LITERAL:  display_node_literal(node, node_title); break;
        case NODE_CALL:               display_node_call(node, node_title, depth, parser); break;
        case NODE_BRK:                display_node_brk(node, node_title); break;
        case NODE_CONT:               display_node_cont(node, node_title); break;
        case NODE_RET:                display_node_ret(node, node_title, depth, parser); break;
        case NODE_STRUCT_DEFINITION:  display_node_structdef(node, node_title); break;
        case NODE_BRACED_EXPRESSION:  display_node_braced_expression(node, node_title, depth, parser); break;
        case NODE_BRANCH:             display_node_branch(node, node_title, depth, parser); break;
        case NODE_IF:                 display_node_if(node, node_title, depth, parser); break;
        case NODE_ELSE:               display_node_else(node, node_title, depth, parser); break;
        case NODE_WHILE:              display_node_while(node, node_title, depth, parser);break;
        case NODE_SWITCH:             display_node_switch(node, node_title, depth, parser);break;
        case NODE_CASE:               display_node_case(node, node_title, depth, parser);break;
        case NODE_DEFAULT:            display_node_default(node, node_title, depth, parser); break;
        case NODE_FOR:                display_node_for(node, node_title, depth, parser); break;
        case NODE_SUBSCRIPT:          display_node_subscript(node, node_title, depth, parser); break;
        case NODE_NAMESPACEDECL:      display_node_namespacedecl(node, node_title, depth, parser); break;

        case NODE_NONE:
            print("{}None", node_title); // Shouldn't ever happen...
            break;

        default:
            print("{}{}", node_title, "?? Unknown Node Type...");
            break;
    }
}


void
parser::dump_nodes() {

    print("-- ABSTRACT SYNTAX TREE -- ");
    for(const auto node : toplevel_decls)
        display_node_data(node, 0, *this);

    print("");
}