//
// Created by Diago on 2024-08-06.
//
// Just one file for now. Expand later.

#include <codegen.hpp>


llvm::Type*
tak::generate_primitive(CodegenContext& ctx, const var_t prim) {
    switch(prim) {
        case VAR_U8:  case VAR_I8:  return llvm::Type::getInt8Ty(ctx.llvm_ctx_);
        case VAR_U16: case VAR_I16: return llvm::Type::getInt16Ty(ctx.llvm_ctx_);
        case VAR_U32: case VAR_I32: return llvm::Type::getInt32Ty(ctx.llvm_ctx_);
        case VAR_U64: case VAR_I64: return llvm::Type::getInt64Ty(ctx.llvm_ctx_);
        case VAR_F32:               return llvm::Type::getFloatTy(ctx.llvm_ctx_);
        case VAR_F64:               return llvm::Type::getDoubleTy(ctx.llvm_ctx_);
        case VAR_BOOLEAN:           return llvm::Type::getInt1Ty(ctx.llvm_ctx_);
        case VAR_VOID:              return llvm::Type::getVoidTy(ctx.llvm_ctx_);
        default: break;
    }

    panic("generate_primitive: default reached.");
}


llvm::Type*
tak::generate_proc_signature(CodegenContext& ctx, const TypeData& type) {

    assert(type.kind == TYPE_KIND_PROCEDURE);

    std::vector<llvm::Type*> parameters;
    llvm::Type* return_type = nullptr;
    const bool  is_variadic = type.flags & TYPE_PROC_VARARGS ? true : false;

    if(type.parameters != nullptr) {
        for(const auto& param : *type.parameters) {
            parameters.emplace_back(generate_type(ctx, param));
        }
    }

    if(type.return_type != nullptr) return_type = generate_type(ctx, *type.return_type);
    if(return_type == nullptr)      return_type = llvm::Type::getVoidTy(ctx.llvm_ctx_);
    if(parameters.empty())          return llvm::FunctionType::get(return_type, is_variadic);

    return llvm::FunctionType::get(return_type, parameters, is_variadic);
}


llvm::Type*
tak::generate_type(CodegenContext& ctx, const TypeData& type) {

    llvm::Type* gen_t = nullptr;

    if(type.kind == TYPE_KIND_VARIABLE)  gen_t = generate_primitive(ctx, std::get<var_t>(type.name));
    if(type.kind == TYPE_KIND_STRUCT)    gen_t = create_struct_type_if_not_exists(ctx, std::get<std::string>(type.name));
    if(type.kind == TYPE_KIND_PROCEDURE) gen_t = generate_proc_signature(ctx, type);

    assert(gen_t != nullptr);
    for(size_t i = 0; i < type.pointer_depth; i++) {
        gen_t = llvm::PointerType::get(gen_t, 0);
    }

    for(auto i = type.array_lengths.rbegin(); i != type.array_lengths.rend(); ++i) {
        gen_t = llvm::ArrayType::get(gen_t, *i);
    }

    return gen_t;
}


llvm::StructType*
tak::create_struct_type_if_not_exists(CodegenContext& ctx, const std::string& name) {
    llvm::StructType* _struct = llvm::StructType::getTypeByName(ctx.llvm_ctx_, name);
    return _struct == nullptr ? llvm::StructType::create(ctx.llvm_ctx_, name) : _struct;
}


void
tak::generate_struct_layouts(CodegenContext& ctx) {
    for(const auto &[name, type] : ctx.tbl_.type_table_) {
        llvm::StructType* _struct = create_struct_type_if_not_exists(ctx, name);
        std::vector<llvm::Type*> elements;

        for(const auto &[name, type] : type->members) {
            const auto& back = elements.emplace_back(generate_type(ctx, type));
            assert(back != nullptr);
        }

        _struct->setBody(elements);
    }
}


llvm::Value*
tak::generate_node(const AstNode* node, llvm::Module&, llvm::LLVMContext&) {

    assert(node != nullptr);
    switch(node->type) {
        default: break;
    }

    panic("tak::generate_node: default reached.");
}