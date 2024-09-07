//
// Created by Diago on 2024-09-03.
//

#ifndef DO_COMPILE_HPP
#define DO_COMPILE_HPP
#include <lexer.hpp>
#include <parser.hpp>
#include <config.hpp>
#include <checker.hpp>
#include <postparser.hpp>
#include <codegen.hpp>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/IR/LegacyPassManager.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace tak {
    bool do_compile();
    bool do_create_ast(Parser& parser);
    bool emit_llvm_ir(CodegenContext& ctx, llvm::TargetMachine* machine);
    bool do_codegen(Parser& parser, const std::string& llvm_mod_name);
    bool do_check(Parser& parser);
    bool do_parse(Parser& parser, Lexer& lexer);
    llvm::TargetMachine* initialize_llvm_target(CodegenContext& ctx);
    bool get_next_include(Parser& parser, Lexer& lexer);
}

#endif //DO_COMPILE_HPP
