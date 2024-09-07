//
// Created by Diago on 2024-09-03.
//

#include <codegen.hpp>


void
tak::generate_global_placeholders(CodegenContext& ctx) {
    for(const auto &[index, sym] : ctx.tbl_.sym_table_) {
        if(!(sym->flags & ENTITY_GLOBAL) || (sym->type.kind == TYPE_KIND_PROCEDURE && !(sym->type.flags & TYPE_POINTER))) {
            continue;
        }

        new llvm::GlobalVariable(
            ctx.mod_,
            generate_type(ctx, sym->type),
            sym->type.flags & TYPE_CONSTANT,
            sym->flags & ENTITY_FOREIGN ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::InternalLinkage,
            nullptr,
            sym->name
        );
    }
}

void
tak::generate_procedure_signatures(CodegenContext& ctx) {
    for(const auto &[index, sym] : ctx.tbl_.sym_table_) {
        if(sym->type.kind != TYPE_KIND_PROCEDURE || sym->type.flags & TYPE_POINTER || sym->type.flags & TYPE_ARRAY) {
            continue;
        }

        if(auto* signature = llvm::dyn_cast<llvm::FunctionType>(generate_type(ctx, sym->type))) {
            auto* func = llvm::Function::Create(
                signature,
                sym->flags & ENTITY_INTERNAL ? llvm::GlobalValue::InternalLinkage : llvm::GlobalValue::ExternalLinkage,
                sym->name,
                ctx.mod_
            );

            if(sym->flags & ENTITY_FOREIGN_C) {
                assert(!sym->name.empty());
                assert(sym->name.front() == '\\');

                sym->name.erase(0,1);
                func->setName(sym->name);
                func->setCallingConv(llvm::CallingConv::C);
            }

            continue;
        }

        panic(fmt("Symbol {} is not an instance of a procedure type.", sym->name));
    }
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
