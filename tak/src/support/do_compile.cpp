//
// Created by Diago on 2024-08-02.
//

#include <io.hpp>
#include <lexer.hpp>
#include <parser.hpp>
#include <checker.hpp>
#include <exception>

using namespace tak;


static bool
check_leftover_placeholders(Parser& parser, Lexer& lexer) {

    bool state = true;
    static constexpr std::string_view msg = "Failed to resolve {} \"{}\", first usage is here.";

    for(const auto &[_, sym] : parser.sym_table_) {
        if(sym.flags & SYM_PLACEHOLDER) {
            state = false;
            lexer.raise_error(fmt(msg, "symbol", sym.name), sym.src_pos, sym.line_number);
        }
    }

    for(const auto &[name, type] : parser.type_table_) {
        if(type.is_placeholder) {
            state = false;
            lexer.raise_error(fmt(msg, "type", name), type.pos_first_used, type.line_first_used);
        }
    }

    return state;
}

static bool
do_parse(Parser& parser, Lexer& lexer) {

    AstNode* toplevel_decl = nullptr;

    if(parser.scope_stack_.empty()) {
        parser.push_scope(); // global scope
    }

    do {
        toplevel_decl = parse_expression(parser, lexer, false);
        if(toplevel_decl == nullptr) {
            break;
        }
        // TODO: verify valid at toplevel
        parser.toplevel_decls_.emplace_back(toplevel_decl);
    } while(true);

    parser.pop_scope();
    if(lexer.current() != TOKEN_END_OF_FILE || !check_leftover_placeholders(parser, lexer)) {
        return false;
    }

    return true;
}

static bool
do_check(Parser& parser, Lexer& lexer) {

    CheckerContext ctx(lexer, parser);
    for(const auto& decl : parser.toplevel_decls_) {
        if(NODE_NEEDS_VISITING(decl->type)) {
            visit_node(decl, ctx);
        }
    }

    if(ctx.error_count_ > 0) {
        print<TFG_RED, TBG_NONE, TSTYLE_BOLD>("\n{}: BUILD FAILED", lexer.source_file_name_);
        print<TFG_NONE, TBG_NONE, TSTYLE_NONE>("Finished with {} errors, {} warnings.", ctx.error_count_, ctx.warning_count_);
        return false;
    }

#ifdef TAK_DEBUG
    parser.dump_nodes();
    parser.dump_symbols();
    parser.dump_types();
#endif

    return true;
}

static bool
do_create_ast(Parser& parser, const std::string& source_file_name) {

    Lexer lexer;
    return lexer.init(source_file_name) && do_parse(parser, lexer) && do_check(parser, lexer);
}

bool
do_compile(const std::string& source_file_name) {

    Parser parser;
    if(!do_create_ast(parser, source_file_name)) return false;
    return true;
}