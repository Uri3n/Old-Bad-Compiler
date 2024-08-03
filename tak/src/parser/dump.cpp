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
    tak::print("{}{}", node_title, name);
}

static void
display_node_vardecl(tak::AstNode* node, std::string& node_title, const uint32_t depth, tak::Parser& _) {

    node_title += "Variable Declaration";
    const auto* vardecl = dynamic_cast<tak::AstVardecl*>(node);

    if(vardecl == nullptr) {
        node_title += " !! INVALID NODE TYPE";
        tak::print("{}", node_title);
        return;
    }


    tak::print("{}", node_title);
    display_node_data(vardecl->identifier, depth + 1, _);
    if(vardecl->init_value.has_value()) {
        display_node_data(*(vardecl->init_value), depth + 1, _);
    }
}

static void
display_node_procdecl(tak::AstNode* node, std::string& node_title, uint32_t depth, tak::Parser& _) {

    const auto* procdecl = dynamic_cast<tak::AstProcdecl*>(node);

    if(procdecl == nullptr) {
        node_title += " !! INVALID NODE TYPE";
        tak::print("{}", node_title);
        return;
    }


    tak::print("{}Procedure Declaration", node_title);
    display_node_data(procdecl->identifier, depth + 1, _);

    if(!procdecl->parameters.empty() || !procdecl->body.empty()) {
        node_title.insert(0, "     ");
        if(!depth) {
            node_title += "|- ";
        }

        ++depth;
    }


    if(!procdecl->parameters.empty()) {
        tak::print("{}Parameters", node_title);
        for(tak::AstNode* param : procdecl->parameters) {
            display_node_data(param, depth + 1, _);
        }
    }

    if(!procdecl->body.empty()) {
        tak::print("{}Procedure Body", node_title);
        for(tak::AstNode* child : procdecl->body) {
            display_node_data(child, depth + 1, _);
        }
    }
}


static void
display_node_binexpr(tak::AstNode* node, std::string& node_title, const uint32_t depth, tak::Parser& _) {

    const auto* binexpr = dynamic_cast<tak::AstBinexpr*>(node);

    if(binexpr == nullptr) {
        node_title += "(Binary Expression) !! INVALID NODE TYPE";
        tak::print("{}", node_title);
        return;
    }

    node_title += token_type_to_string(binexpr->_operator);
    tak::print("{} (Binary Expression)", node_title);

    display_node_data(binexpr->left_op, depth + 1, _);
    display_node_data(binexpr->right_op, depth + 1, _);
}

static void
display_node_unaryexpr(tak::AstNode* node, std::string& node_title, const uint32_t depth, tak::Parser& _) {

    const auto* unaryexpr = dynamic_cast<tak::AstUnaryexpr*>(node);

    if(unaryexpr == nullptr) {
        node_title += "(Unary Expression) !! INVALID NODE TYPE";
        tak::print("{}", node_title);
        return;
    }

    node_title += tak::token_type_to_string(unaryexpr->_operator);
    tak::print("{} (Unary Expression)", node_title);

    display_node_data(unaryexpr->operand, depth + 1, _);
}

static void
display_node_identifier(tak::AstNode* node, std::string& node_title, tak::Parser& parser) {

    const auto* ident = dynamic_cast<tak::AstIdentifier*>(node);

    if(ident == nullptr) {
        node_title += "(Ident) !! INVALID NODE TYPE";
        tak::print("{}", node_title);
        return;
    }

    const tak::Symbol* sym_ptr = parser.lookup_unique_symbol(ident->symbol_index);
    if(sym_ptr == nullptr) {
        node_title += tak::fmt("(Ident) (Sym Index {}) !! NOT IN SYMBOL TABLE", ident->symbol_index);
        tak::print("{}", node_title);
        return;
    }


    std::string sym_t_str;
    if(sym_ptr->type.kind == tak::TYPE_KIND_PROCEDURE) {
        sym_t_str = "Procedure";
    } else if(sym_ptr->type.kind == tak::TYPE_KIND_VARIABLE) {
        sym_t_str = "Variable";
    } else if(sym_ptr->type.kind == tak::TYPE_KIND_STRUCT){
        sym_t_str = "Struct";
    } else {
        sym_t_str = "Inferred";
    }


    node_title += tak::fmt(
        "{} ({}) (Sym Index {})",
        sym_ptr->name,
        sym_t_str,
        sym_ptr->symbol_index
    );

    tak::print("{}", node_title);
}

static void
display_node_literal(tak::AstNode* node, std::string& node_title) {

    const auto* lit = dynamic_cast<tak::AstSingletonLiteral*>(node);

    if(lit == nullptr) {
        node_title += "(Literal) !! INVALID NODE TYPE";
        tak::print("{}", node_title);
        return;
    }

    node_title += tak::fmt("{} ({})", lit->value, token_type_to_string(lit->literal_type));
    tak::print("{}", node_title);
}

static void
display_node_call(tak::AstNode* node, std::string& node_title, uint32_t depth, tak::Parser& parser) {

    const auto* call = dynamic_cast<tak::AstCall*>(node);
    if(call == nullptr) {
        tak::print("{} (Call) !! INVALID NODE TYPE", node_title);
        return;
    }


    tak::print("{}Procedure Call", node_title);
    display_node_data(call->target, depth + 1, parser);

    if(!call->arguments.empty()) {
        display_fake_node("Arguments", node_title, depth);
    }

    for(tak::AstNode* arg : call->arguments) {
        display_node_data(arg, depth + 1, parser);
    }
}

static void
display_node_brk(tak::AstNode* node, const std::string& node_title) {

    const auto* brk = dynamic_cast<tak::AstBrk*>(node);
    if(brk == nullptr) {
        tak::print("{} (Break) !! INVALID NODE TYPE", node_title);
        return;
    }

    tak::print("{}Break Statement", node_title);
}

static void
display_node_cont(tak::AstNode* node, const std::string& node_title) {

    const auto* cont = dynamic_cast<tak::AstCont*>(node);
    if(cont == nullptr) {
        tak::print("{} (Continue) !! INVALID NODE TYPE", node_title);
        return;
    }

    tak::print("{}Continue Statement", node_title);
}

static void
display_node_ret(tak::AstNode* node, const std::string& node_title, const uint32_t depth, tak::Parser& _) {

    const auto* ret = dynamic_cast<tak::AstRet*>(node);
    if(ret == nullptr) {
        tak::print("{} (Return) !! INVALID NODE TYPE", node_title);
        return;
    }

    tak::print("{}Return", node_title);
    if(ret->value.has_value()) {
        display_node_data(*ret->value, depth + 1, _);
    }
}

static void
display_node_structdef(tak::AstNode* node, const std::string& node_title) {

    const auto* _struct = dynamic_cast<tak::AstStructdef*>(node);
    if(_struct == nullptr) {
        tak::print("{} (Struct Definition) !! INVALID NODE TYPE", node_title);
        return;
    }

    tak::print("{}{} (Struct Definition)", node_title, _struct->name);
}

static void
display_node_braced_expression(tak::AstNode* node, const std::string& node_title, const uint32_t depth, tak::Parser& _) {

    const auto* expr = dynamic_cast<tak::AstBracedExpression*>(node);
    if(expr == nullptr) {
        tak::print("{} (Braced Expression) !! INVALID NODE TYPE", node_title);
        return;
    }

    tak::print("{}Braced Expression", node_title);
    for(tak::AstNode* member : expr->members) {
        display_node_data(member, depth + 1, _);
    }
}

static void
display_node_branch(tak::AstNode* node, const std::string& node_title, const uint32_t depth, tak::Parser& _) {

    const auto* branch = dynamic_cast<tak::AstBranch*>(node);
    if(branch == nullptr) {
        tak::print("{} (Branch) !! INVALID NODE TYPE", node_title);
        return;
    }

    tak::print("{}Branch", node_title);
    for(tak::AstNode* if_stmt : branch->conditions) {
        display_node_data(if_stmt, depth + 1, _);
    }

    if(branch->_else.has_value()) {
        display_node_data(*branch->_else, depth + 1, _);
    }
}

static void
display_node_if(tak::AstNode* node, std::string& node_title, uint32_t depth, tak::Parser& _) {

    const auto* if_stmt = dynamic_cast<tak::AstIf*>(node);
    if(if_stmt == nullptr) {
        tak::print("{} (If) !! INVALID NODE TYPE", node_title);
        return;
    }

    tak::print("{}If", node_title);
    display_node_data(if_stmt->condition, depth + 1, _);
    display_fake_node("Body", node_title, depth);

    for(tak::AstNode* expr :  if_stmt->body) {
        display_node_data(expr, depth + 1, _);
    }
}

static void
display_node_while(tak::AstNode* node, std::string& node_title, uint32_t depth, tak::Parser& _) {

    const auto* _while = dynamic_cast<tak::AstWhile*>(node);
    if(_while == nullptr) {
        tak::print("{} (While) !! INVALID NODE TYPE", node_title);
        return;
    }

    tak::print("{}While", node_title);
    display_node_data(_while->condition, depth + 1, _);
    display_fake_node("Body", node_title, depth);

    for(tak::AstNode* expr : _while->body) {
        display_node_data(expr, depth + 1, _);
    }
}

static void
display_node_else(tak::AstNode* node, std::string& node_title, uint32_t depth, tak::Parser& _) {

    const auto* else_stmt = dynamic_cast<tak::AstElse*>(node);
    if(else_stmt == nullptr) {
        tak::print("{} (Else) !! INVALID NODE TYPE", node_title);
        return;
    }

    tak::print("{}Else", node_title);
    display_fake_node("Body", node_title, depth);

    for(tak::AstNode* expr :  else_stmt->body) {
        display_node_data(expr, depth + 1, _);
    }
}

static void
display_node_case(tak::AstNode* node, std::string& node_title, uint32_t depth, tak::Parser& _) {

    const auto* _case = dynamic_cast<tak::AstCase*>(node);
    if(_case == nullptr) {
        tak::print("{} (Default) !! INVALID NODE TYPE", node_title);
        return;
    }

    tak::print("{}Case {} (Fallthrough={})", node_title, _case->value->value, _case->fallthrough ? "True" : "False");
    display_fake_node("Body", node_title, depth);

    for(tak::AstNode* expr : _case->body) {
        display_node_data(expr, depth + 1, _);
    }
}

static void
display_node_default(tak::AstNode* node, std::string& node_title, uint32_t depth, tak::Parser& _) {

    const auto* _default = dynamic_cast<tak::AstDefault*>(node);
    if(_default == nullptr) {
        tak::print("{} (Default) !! INVALID NODE TYPE", node_title);
        return;
    }

    tak::print("{}Default", node_title);
    display_fake_node("Body", node_title, depth);

    for(tak::AstNode* expr : _default->body) {
        display_node_data(expr, depth + 1, _);
    }
}

static void
display_node_switch(tak::AstNode* node, const std::string& node_title, const uint32_t depth, tak::Parser& _) {

    const auto* _switch = dynamic_cast<tak::AstSwitch*>(node);
    if(_switch == nullptr) {
        tak::print("{} (Switch) !! INVALID NODE TYPE", node_title);
        return;
    }

    tak::print("{}Switch", node_title);

    display_node_data(_switch->target, depth + 1, _);
    for(tak::AstNode* _case : _switch->cases) {
        display_node_data(_case, depth + 1, _);
    }

    display_node_data(_switch->_default, depth + 1, _);
}

static void
display_node_for(tak::AstNode* node, std::string& node_title, uint32_t depth, tak::Parser& _) {

    const auto* _for = dynamic_cast<tak::AstFor*>(node);
    if(_for == nullptr) {
        tak::print("{} (For) !! INVALID NODE TYPE", node_title);
        return;
    }

    tak::print("{}For", node_title);

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
    for(tak::AstNode* expr : _for->body) {
        display_node_data(expr, depth + 1, _);
    }
}

static void
display_node_subscript(tak::AstNode* node, const std::string& node_title, const uint32_t depth, tak::Parser& _) {

    const auto* subscript = dynamic_cast<tak::AstSubscript*>(node);
    if(subscript == nullptr) {
        tak::print("{} (For) !! INVALID NODE TYPE", node_title);
        return;
    }

    tak::print("{}Index Into (Subscript)", node_title);
    display_node_data(subscript->operand, depth + 1, _);
    display_node_data(subscript->value, depth + 1, _);
}

static void
display_node_block(tak::AstNode* node, const std::string& node_title, const uint32_t depth, tak::Parser& _) {

    const auto* block = dynamic_cast<tak::AstBlock*>(node);
    if(block == nullptr) {
        tak::print("{} (Block) !! INVALID NODE TYPE", node_title);
        return;
    }

    tak::print("{}Scope Block", node_title);
    for(tak::AstNode* child : block->body) {
        display_node_data(child, depth + 1, _);
    }
}

static void
display_node_namespacedecl(tak::AstNode* node, const std::string& node_title, const uint32_t depth, tak::Parser& _) {

    const auto* _namespace = dynamic_cast<tak::AstNamespaceDecl*>(node);
    if(_namespace == nullptr) {
        tak::print("{} (For) !! INVALID NODE TYPE", node_title);
        return;
    }

    tak::print("{}{} (Namespace Decl)", node_title, _namespace->full_path);
    for(tak::AstNode* expr : _namespace->children) {
        display_node_data(expr, depth + 1, _);
    }
}

static void
display_node_dowhile(tak::AstNode* node, const std::string& node_title, const uint32_t depth, tak::Parser& _) {

    const auto* dowhile = dynamic_cast<tak::AstDoWhile*>(node);
    if(dowhile == nullptr) {
        tak::print("{} (Do-While) !! INVALID NODE TYPE", node_title);
        return;
    }

    tak::print("{}Do", node_title);
    uint32_t    tmp_depth = depth;
    std::string tmp_title = node_title;

    display_fake_node("Body", tmp_title, tmp_depth);
    for(tak::AstNode* expr : dowhile->body) {
        display_node_data(expr, tmp_depth + 1, _);
    }

    tmp_depth = depth;
    tmp_title = node_title;

    display_fake_node("While", tmp_title, tmp_depth);
    display_node_data(dowhile->condition, tmp_depth + 1, _);
}

static void
display_node_cast(tak::AstNode* node, std::string& node_title, uint32_t depth, tak::Parser& _) {

    const auto* cast = dynamic_cast<tak::AstCast*>(node);
    if(cast == nullptr) {
        tak::print("{} (Type Cast) !! INVALID NODE TYPE", node_title);
        return;
    }

    tak::print("{}Type Cast", node_title);
    display_node_data(cast->target, depth + 1, _);


    //
    // Type data displayed is not detailed at all... should be ok though
    //

    std::string type_name;
    if(const auto* is_var = std::get_if<tak::var_t>(&cast->type.name)) {
        type_name = tak::var_t_to_string(*is_var);
    } else if(const auto* is_struct = std::get_if<std::string>(&cast->type.name)) {
        type_name = tak::fmt("{} (Structure)", *is_struct);
    } else {
        type_name = "Procedure";
    }

    display_fake_node(tak::fmt("Type: {}", type_name), node_title, depth);
}

static void
display_node_enumdef(tak::AstNode* node, const std::string& node_title, const uint32_t depth, tak::Parser& _) {

    const auto* _enum = dynamic_cast<tak::AstEnumdef*>(node);
    if(_enum == nullptr) {
        tak::print("{} (Enum Definition) !! INVALID NODE TYPE", node_title);
        return;
    }

    tak::print("{}{} (Enum Definition)", node_title, _enum->alias->name);
    display_node_data(_enum->_namespace, depth + 1, _);
    display_node_data(_enum->alias, depth + 1, _);
}

static void
display_node_defer(tak::AstNode* node, const std::string& node_title, const uint32_t depth, tak::Parser& _) {

    const auto* defer_stmt = dynamic_cast<tak::AstDefer*>(node);
    if(defer_stmt == nullptr) {
        tak::print("{} (Defer Statement) !! INVALID NODE TYPE", node_title);
        return;
    }

    tak::print("{}defer", node_title);
    display_node_data(defer_stmt->call, depth + 1, _);
}

static void
display_node_defer_if(tak::AstNode* node, const std::string& node_title, const uint32_t depth, tak::Parser& _) {

    const auto* defer_stmt = dynamic_cast<tak::AstDeferIf*>(node);
    if(defer_stmt == nullptr) {
        tak::print("{} (defer_if Statement) !! INVALID NODE TYPE", node_title);
        return;
    }

    tak::print("{}defer_if", node_title);
    display_node_data(defer_stmt->condition, depth + 1, _);
    display_node_data(defer_stmt->call, depth + 1, _);
}

static void
display_node_type_alias(tak::AstNode* node, const std::string& node_title, tak::Parser& parser) {

    const auto* alias = dynamic_cast<tak::AstTypeAlias*>(node);
    if(alias == nullptr) {
        tak::print("{} (Type Alias Definition) !! INVALID NODE TYPE", node_title);
        return;
    }

    tak::print("{}{}: Type Alias Definition, Expands To {}",
        node_title,
        alias->name,
        typedata_to_str_msg(parser.lookup_type_alias(alias->name))
    );
}

static void
display_node_member_access(tak::AstNode* node, const std::string& node_title, const uint32_t depth, tak::Parser& _) {

    const auto* member = dynamic_cast<tak::AstMemberAccess*>(node);
    if(member == nullptr) {
        tak::print("{} (Member Access) !! INVALID NODE TYPE", node_title);
        return;
    }

    tak::print("{}Member Access ({})", node_title, member->path);
    display_node_data(member->target, depth + 1, _);
}

static void
display_node_sizeof(tak::AstNode* node, std::string& node_title, uint32_t depth, tak::Parser& _) {

    const auto* _sizeof = dynamic_cast<tak::AstSizeof*>(node);
    if(_sizeof == nullptr) {
        tak::print("{} (Type Alias Definition) !! INVALID NODE TYPE", node_title);
        return;
    }


    tak::print("{}SizeOf", node_title);

    if(const auto* is_raw_type = std::get_if<tak::TypeData>(&_sizeof->target)) {
        display_fake_node(tak::fmt("Type: {}", typedata_to_str_msg(*is_raw_type)), node_title, depth);
    }
    else if(const auto* is_node = std::get_if<tak::AstNode*>(&_sizeof->target)) {
        display_node_data(*is_node, depth + 1, _);
    }
    else {
        display_fake_node("?? Bad variant type", node_title, depth);
    }
}

void
tak::display_node_data(AstNode* node, const uint32_t depth, Parser& parser) {

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
        case NODE_BLOCK:              display_node_block(node, node_title, depth, parser); break;
        case NODE_DOWHILE:            display_node_dowhile(node, node_title, depth,parser); break;
        case NODE_CAST:               display_node_cast(node, node_title, depth, parser); break;
        case NODE_TYPE_ALIAS:         display_node_type_alias(node, node_title, parser); break;
        case NODE_ENUM_DEFINITION:    display_node_enumdef(node, node_title, depth, parser); break;
        case NODE_DEFER:              display_node_defer(node, node_title, depth, parser); break;
        case NODE_DEFER_IF:           display_node_defer_if(node, node_title, depth, parser); break;
        case NODE_SIZEOF:             display_node_sizeof(node, node_title, depth, parser); break;
        case NODE_MEMBER_ACCESS:      display_node_member_access(node, node_title, depth, parser); break;

        case NODE_NONE:
            print("{}None", node_title);                            // Shouldn't ever happen...
            break;

        default:
            print("{}{}", node_title, "?? Unknown Node Type...");   // Shouldn't ever happen...
            break;
    }
}

void
tak::Parser::dump_nodes() {

    print("-- ABSTRACT SYNTAX TREE -- ");
    for(const auto node : toplevel_decls_)
        display_node_data(node, 0, *this);

    print("");
}

void
tak::Parser::dump_types() {

    if(type_table_.empty()) {
        print("No user-defined types exist.");
        return;
    }

    print(" -- USER DEFINED TYPES -- ");
    for(const auto &[name, type] : type_table_) {
        print("~ {}{} ~\n  Members:", name, type.is_placeholder ? " (Placeholder)" : "");
        for(size_t i = 0; i < type.members.size(); ++i) {
            print("    {}. {}", std::to_string(i + 1), type.members[i].name);
            print("{}", format_type_data(type.members[i].type, 1));
        }
    }

    print("");
}

std::string
tak::format_type_data(const TypeData& type, const uint16_t num_tabs) {

    static constexpr std::string_view fmt_type =
        "{} - Symbol Type:   {}"
        "\n{} - Flags:         {}"
        "\n{} - Pointer Depth: {}"
        "\n{} - Matrix Depth:  {}"
        "\n{} - Array Lengths: {}"
        "\n{} - Type Name:     {}"
        "\n{} - Return Type:   {}"
        "\n{} - Parameters:    {}\n";

    std::string tabs;
    for(uint16_t i = 0; i < num_tabs; i++) {
        tabs += '\t';
    }


    std::string flags;
    if(type.flags & TYPE_CONSTANT)         flags += "CONSTANT | ";
    if(type.flags & TYPE_FOREIGN)          flags += "FOREIGN | ";
    if(type.flags & TYPE_POINTER)          flags += "POINTER | ";
    if(type.flags & TYPE_GLOBAL)           flags += "GLOBAL | ";
    if(type.flags & TYPE_ARRAY)            flags += "ARRAY | ";
    if(type.flags & TYPE_PROCARG)          flags += "PROCEDURE_ARGUMENT | ";
    if(type.flags & TYPE_UNINITIALIZED)    flags += "UNINITIALIZED | ";
    if(type.flags & TYPE_DEFAULT_INIT)     flags += "DEFAULT INITIALIZED | ";
    if(type.flags & TYPE_INFERRED)         flags += "INFERRED";

    if(flags.size() >= 2 && flags[flags.size()-2] == '|') {
        flags.erase(flags.size()-2);
    } if(flags.empty()) {
        flags = "None";
    }


    std::string sym_t_str;
    if(type.kind == TYPE_KIND_PROCEDURE)     sym_t_str = "Procedure";
    else if(type.kind == TYPE_KIND_VARIABLE) sym_t_str = "Variable";
    else if(type.kind == TYPE_KIND_STRUCT)   sym_t_str = "Struct";
    else if(type.kind == TYPE_KIND_NONE)     sym_t_str = "None";

    std::string type_name_str;
    if(auto is_var = std::get_if<var_t>(&type.name)) {
        type_name_str = var_t_to_string(*is_var);
    } else if(auto is_struct = std::get_if<std::string>(&type.name)) {
        type_name_str = fmt("{} (User Defined Struct)", *is_struct);
    } else {
        type_name_str = "Procedure";
    }


    std::string return_type_data;
    std::string param_data;

    if(type.return_type != nullptr) {
        return_type_data = "\n\n~~ BEGIN RETURN TYPE ~~\n";
        return_type_data += format_type_data(*type.return_type, num_tabs + 1);
        return_type_data += "\n~~ END RETURN TYPE ~~\n";
    } else {
        return_type_data = "N/A";
    }

    if(type.parameters != nullptr) {
        param_data = "\n\n~~ BEGIN PARAMETERS ~~\n";
        for(const auto& param : *type.parameters) {
            param_data += format_type_data(param, num_tabs + 1) + '\n';
        }
        param_data += "~~ END PARAMETERS ~~\n";
    } else {
        param_data = "N/A";
    }


    std::string array_lengths;
    for(const auto& len : type.array_lengths)
        array_lengths += std::to_string(len) + ',';

    if(!array_lengths.empty() && array_lengths.back() == ',')
        array_lengths.pop_back();


    return fmt(fmt_type,
        tabs,
        sym_t_str,
        tabs,
        flags,
        tabs,
        type.pointer_depth,
        tabs,
        type.array_lengths.size(),
        tabs,
        array_lengths.empty() ? "N/A" : array_lengths,
        tabs,
        type_name_str,
        tabs,
        return_type_data,
        tabs,
        param_data
    );
}

void
tak::Parser::dump_symbols() {

    static constexpr std::string_view fmt_sym =
        "~ {}{} ~"
        "\n - Symbol Index:  {}"
        "\n - Line Number:   {}"
        "\n - File Position: {}"
        "\n{}"; //< type data

    print(" -- SYMBOL TABLE -- ");
    for(const auto &[index, sym] : sym_table_) {
        print(fmt_sym,
            sym.name,
            sym.placeholder ? " (Placeholder)" : "",
            sym.symbol_index,
            sym.line_number,
            sym.src_pos,
            format_type_data(sym.type)
        );
    }

    print("");
}