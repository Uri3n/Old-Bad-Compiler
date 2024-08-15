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

#define NODE_NEEDS_GENERATING(node_type)      \
   (node_type != tak::NODE_STRUCT_DEFINITION  \
    && node_type != tak::NODE_INCLUDE_STMT    \
    && node_Type != tak::NODE_TYPE_ALIAS      \
)                                             \

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace tak {

    class CodegenContext {
    public:
        EntityTable& tbl_;           // Reference to the Tak entity table for this source file
        llvm::LLVMContext llvm_ctx_; // LLVM context
        llvm::Module mod_;           // LLVM module, same name as source file

        ~CodegenContext() = default;
        explicit CodegenContext(EntityTable& tbl, const std::string& module_name)
            : tbl_(tbl), llvm_ctx_(), mod_(module_name, llvm_ctx_) {}
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    llvm::Type* generate_proc_signature(CodegenContext& ctx, const TypeData& type);
    llvm::Type* generate_primitive(CodegenContext& ctx, var_t prim);
    llvm::Type* generate_type(CodegenContext& ctx, const TypeData& type);
    llvm::StructType* create_struct_type_if_not_exists(CodegenContext& ctx, const std::string& name);
    void generate_struct_layouts(CodegenContext& ctx);

    llvm::Value* generate_node(const AstNode* node, llvm::Module& mod, llvm::LLVMContext& ctx);
}

#endif //CODEGEN_HPP
