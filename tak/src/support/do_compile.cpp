//
// Created by Diago on 2024-08-02.
//

#include <do_compile.hpp>


bool
tak::get_next_include(Parser& parser, Lexer& lexer) {
    const auto next_include = std::ranges::find_if(parser.included_files_, [](const IncludedFile& f) {
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

bool
tak::do_parse(Parser& parser, Lexer& lexer) {
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
            if(!NODE_VALID_AT_TOPLEVEL(toplevel_decl->type)) {
                lexer.raise_error("Expression is invalid at the top level.", toplevel_decl->src_pos, toplevel_decl->line);
                return false;
            }
            parser.toplevel_decls_.emplace_back(toplevel_decl);
        }
        if(lexer.current() != TOKEN_END_OF_FILE) {
            return false;
        }
    } while(get_next_include(parser, lexer));

    return postparse_verify(parser, lexer);
}

bool
tak::do_check(Parser& parser) {
    CheckerContext ctx(parser.tbl_);
    for(const auto& decl : parser.toplevel_decls_) {
        if(NODE_NEEDS_EVALUATING(decl->type)) evaluate(decl, ctx);
    }

    ctx.errs_.emit();
    if(ctx.errs_.failed()) {
        red_bold("\n{}: BUILD FAILED", Config::get().input_file_name());
        red_bold("Finished with {} errors, {} warnings.", ctx.errs_.error_count_, ctx.errs_.warning_count_);
    }

    return !ctx.errs_.failed();
}

llvm::TargetMachine*
tak::initialize_llvm_target(CodegenContext &ctx) {
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    std::string err;
    const auto default_triple = Config::get().arch().value_or(llvm::sys::getDefaultTargetTriple());
    const auto* target        = llvm::TargetRegistry::lookupTarget(default_triple, err);

    if(target == nullptr) {
        print<TFG_RED>("\nFailed to initialize LLVM compilation target.");
        print<TFG_RED>("LLVM error message: \"{}\"", err);
        return nullptr;
    }

    const llvm::TargetOptions opt;
    llvm::TargetMachine*      machine = target->createTargetMachine(default_triple, "generic", "", opt, std::nullopt);
    const llvm::DataLayout    layout  = machine->createDataLayout();

    ctx.mod_.setTargetTriple(default_triple);
    ctx.mod_.setDataLayout(layout);
    return machine;
}

bool
tak::emit_llvm_ir(CodegenContext &ctx, llvm::TargetMachine* machine) {
    std::error_code ec;
    const std::string output_file = Config::get().output_file_name() + ".o";
    llvm::raw_fd_ostream dest(output_file, ec, llvm::sys::fs::OF_None);

    if(ec) {
        red_bold("LLVM failed to open output file \"{}\".", output_file);
        print("Error message: \"{}\"", ec.message());
        return false;
    }

    llvm::legacy::PassManager legacy_pass_manager;
    if(machine->addPassesToEmitFile(legacy_pass_manager, dest, nullptr, llvm::CodeGenFileType::ObjectFile)) {
        red_bold("LLVM refused to emit object code.");
        return false;
    }

    legacy_pass_manager.run(ctx.mod_);
    dest.flush();

    print<TFG_GREEN>("\nSuccess.");
    print("Wrote output to: {}", output_file);
    return true;
}

bool
tak::do_codegen(Parser& parser, const std::string& llvm_mod_name) {
    auto  ctx     = CodegenContext(parser.tbl_, llvm_mod_name);
    auto* machine = initialize_llvm_target(ctx);

    if(machine == nullptr) {
        return false;
    }

    generate_struct_layouts(ctx);
    generate_global_placeholders(ctx);
    generate_procedure_signatures(ctx);

    for(auto* child : parser.toplevel_decls_) {
        if(NODE_NEEDS_GENERATING(child->type)) generate(child, ctx);
    }

    const bool verify_result = llvm::verifyModule(ctx.mod_, &llvm::errs());
    if(verify_result == true) {
        panic("LLVM failed to verify the module."); // should never happen
    }

    const uint32_t flags = Config::get().flags();
    if(flags & CONFIG_DUMP_AST)     parser.dump_nodes();
    if(flags & CONFIG_DUMP_SYMBOLS) parser.tbl_.dump_symbols();
    if(flags & CONFIG_DUMP_TYPES)   parser.tbl_.dump_types();
    if(flags & CONFIG_DUMP_LLVM)    ctx.mod_.print(llvm::outs(), nullptr);

    return emit_llvm_ir(ctx, machine);
}

bool
tak::do_create_ast(Parser& parser) {
    Lexer lexer;
    return lexer.init(Config::get().input_file_name())
        && do_parse(parser, lexer)
        && do_check(parser);
}

bool
tak::do_compile() {
    Parser parser;
    const auto source_file_name = Config::get().input_file_name();
    return do_create_ast(parser) && do_codegen(parser, source_file_name);
}
