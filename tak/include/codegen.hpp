//
// Created by Diago on 2024-08-06.
//

#ifndef CODEGEN_HPP
#define CODEGEN_HPP
#include <io.hpp>
#include <parser.hpp>
#include <llvm/ADT/APInt.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace tak {
    llvm::Value* generate_node(const AstNode* node, llvm::Module& mod, llvm::LLVMContext& ctx);
    void generate_struct_types(llvm::Module& mod, llvm::LLVMContext& ctx);
}

#endif //CODEGEN_HPP
