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
            toplevel_decl = parse(parser, lexer, false);
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

#ifdef TAK_DEBUG
    constexpr bool dump = true;
#else
    constexpr bool dump = false;
#endif

    defer_if(dump, [&] {
        parser.dump_nodes();
        parser.tbl_.dump_symbols();
        parser.tbl_.dump_types();
    });

    CheckerContext ctx(parser.tbl_);
    for(const auto& decl : parser.toplevel_decls_) {
        if(NODE_NEEDS_EVALUATING(decl->type)) {
            evaluate(decl, ctx);
        }
    }

    ctx.errs_.emit();
    if(ctx.errs_.failed()) {
        print<TFG_RED, TBG_NONE, TSTYLE_BOLD>("\n{}: BUILD FAILED", original_file_name);
        print<TFG_NONE, TBG_NONE, TSTYLE_NONE>("Finished with {} errors, {} warnings.", ctx.errs_.error_count_, ctx.errs_.warning_count_);
        return false;
    }

    return true;
}

static void
do_codegen(Parser& parser, const std::string& llvm_mod_name) {

    CodegenContext ctx(parser.tbl_, llvm_mod_name);
    ctx.mod_.setDataLayout("e-m:e-i64:64-i128:128-n32:64-S128");

    generate_struct_layouts(ctx);
    generate_global_placeholders(ctx);
    generate_procedure_signatures(ctx);

    for(auto* child : parser.toplevel_decls_) {
        if(NODE_NEEDS_GENERATING(child->type)) generate(child, ctx);
    }

    ctx.mod_.print(llvm::outs(), nullptr);
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
    //do_codegen(parser, source_file_name);

    return true;
}