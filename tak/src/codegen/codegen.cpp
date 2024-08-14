//
// Created by Diago on 2024-08-06.
//
// Just one file for now. Expand later.

#include <codegen.hpp>


llvm::Value*
tak::generate_node(const AstNode* node, llvm::Module&, llvm::LLVMContext&) {

    assert(node != nullptr);
    switch(node->type) {
        default: break;
    }

    panic("tak::generate_node: default reached.");
}