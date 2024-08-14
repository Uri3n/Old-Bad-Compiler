//
// Created by Diago on 2024-08-02.
//

#include <io.hpp>
#include <lexer.hpp>
#include <parser.hpp>
#include <checker.hpp>
#include <postparser.hpp>
#include <codegen.hpp>

using namespace tak;


static bool
get_next_include(Parser& parser, Lexer& lexer) {

    const auto next_include = std::find_if(parser.included_files_.begin(), parser.included_files_.end(), [](const IncludedFile& f) {
        return f.state == INCLUDE_STATE_PENDING;
    });

    if(next_include == parser.included_files_.end()) {
        return false;
    }

    if(!lexer.init(next_include->name)) {
        return false;
    }

    next_include->state = INCLUDE_STATE_DONE;
    return true;
 }


static bool
do_parse(Parser& parser, Lexer& lexer) {

    AstNode* toplevel_decl = nullptr;
    if(parser.tbl_.scope_stack_.empty()) {
        parser.tbl_.push_scope(); // global scope
    }

    do {
        while(true) {
            toplevel_decl = parse_expression(parser, lexer, false);
            if(toplevel_decl == nullptr) {
                break;
            }
            // TODO: verify valid at toplevel
            parser.toplevel_decls_.emplace_back(toplevel_decl);
        }

        if(lexer.current() != TOKEN_END_OF_FILE) {
            return false;
        }

    } while(get_next_include(parser, lexer));

    if(!postparse_verify(parser, lexer)) {
        return false;
    }

    return true;
}

static bool
do_check(Parser& parser, const std::string& original_file_name) {

    CheckerContext ctx(parser.tbl_);
    for(const auto& decl : parser.toplevel_decls_) {
        if(NODE_NEEDS_VISITING(decl->type)) {
            visit_node(decl, ctx);
        }
    }

    ctx.errs_.emit();
    if(ctx.errs_.failed()) {
        print<TFG_RED, TBG_NONE, TSTYLE_BOLD>("\n{}: BUILD FAILED", original_file_name);
        print<TFG_NONE, TBG_NONE, TSTYLE_NONE>("Finished with {} errors, {} warnings.", ctx.errs_.error_count_, ctx.errs_.warning_count_);
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
do_codegen(Parser& parser, const std::string& llvm_mod_name) {

    llvm::LLVMContext ctx;
    llvm::Module(llvm_mod_name, ctx);

    return true;
}

static bool
do_create_ast(Parser& parser, const std::string& source_file_name) {

    Lexer lexer;
    return lexer.init(source_file_name) && do_parse(parser, lexer) && do_check(parser, source_file_name);
}

bool
do_compile(const std::string& source_file_name) {

    Parser parser;
    if(!do_create_ast(parser, source_file_name)) return false;

    return true;
}