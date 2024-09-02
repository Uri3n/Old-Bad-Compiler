//
// Created by Diago on 2024-08-12.
//

#include <postparser.hpp>


bool
tak::postparse_reparse_procedure_permutation(const Symbol* old, Symbol* perm, Parser& parser, Lexer& lxr) {
    assert(old != nullptr);
    assert(perm != nullptr);
    assert(!old->_namespace.empty());
    assert(parser.tbl_.namespace_stack_.empty());
    assert(old->src_pos && old->line_number);
    assert(parser.tbl_.scope_stack_.size() == 1);

    perm->flags        = old->flags;
    perm->flags       &= ~ENTITY_GENBASE;
    perm->type.flags  |= old->type.flags | ENTITY_INTERNAL;
    perm->src_pos      = old->src_pos;
    perm->line_number  = old->line_number;
    perm->file         = old->file;
    perm->_namespace   = old->_namespace;
    perm->type.sym_ref = old->symbol_index;


    if(old->file != lxr.source_file_name_) {
        assert(lxr.init(old->file));
    }

    if(old->flags & ENTITY_FOREIGN) {
        lxr.raise_error("Generic procedures cannot be marked as external.", old->src_pos, old->line_number);
        return false;
    }

    if(old->generic_type_names.size() != perm->type.parameters->size()) {
        lxr.raise_error(fmt("This is being called from file {}, line {} with the wrong number of generic parameters (takes {}, given {}).",
            perm->file,
            perm->line_number,
            old->generic_type_names.size(),
            perm->type.parameters->size()),
            old->src_pos,
            old->line_number
        );
        return false;
    }

    parser.tbl_.push_scope();
    for(size_t i = 0; i < perm->type.parameters->size(); i++) {
        assert(parser.tbl_.create_type_alias(old->generic_type_names[i], perm->type.parameters->at(i)));
    }

    if(old->_namespace != "\\") {
        const std::vector<std::string> namespaces = split_string(old->_namespace, '\\');
        for(const auto& name : namespaces) {
            assert(parser.tbl_.enter_namespace(name));
        }
    }


    bool  state      = false;
    auto* node       = new AstProcdecl(old->src_pos, old->line_number, old->file);
    node->identifier = new AstIdentifier(old->src_pos, old->line_number, old->file);

    node->identifier->symbol_index = perm->symbol_index;
    node->identifier->parent       = node;

    defer([&] {
        parser.tbl_.namespace_stack_.clear();
        parser.tbl_.pop_scope();
        if(!state) delete node;

        for(size_t i = 0; i < old->generic_type_names.size(); i++) {
            parser.tbl_.delete_type_alias(old->generic_type_names[i]);
        }
    });


    lxr.src_index_       = old->src_pos;
    lxr.curr_line_       = old->line_number;
    lxr.current_.line    = lxr.curr_line_;
    lxr.current_.src_pos = lxr.src_index_;
    lxr.current_.kind    = KIND_UNSPECIFIC;
    lxr.current_.type    = TOKEN_NONE;

    while(lxr.current() != TOKEN_LPAREN) {
        lxr.advance(1);
        assert(lxr.current() != TOKEN_END_OF_FILE);
    }

    perm->type.parameters.reset();
    perm->type.parameters = nullptr;

    if(!parse_proc_signature_and_body(perm, node, parser, lxr)) {
        return false;
    }

    parser.toplevel_decls_.emplace_back(node);
    state = true;
    return true;
}


bool
tak::postparse_permute_generic_procedures(Parser& parser, Lexer& lxr) {
    SemanticErrorHandler errs;
    while (true) {
        Symbol* gen_sym = nullptr;
        for(auto &[index, sym] : parser.tbl_.sym_table_) {
            if(sym->flags & ENTITY_GENPERM) {
                gen_sym = sym;
                break;
            }
        }

        if(gen_sym == nullptr) {
            break;
        }

        gen_sym->flags &= ~ENTITY_GENPERM;
        const Symbol* gen_base = parser.tbl_.lookup_unique_symbol(gen_sym->type.sym_ref);
        if(gen_base->generic_type_names.empty() || gen_base->type.kind != TYPE_KIND_PROCEDURE) {
            errs.raise_error("Attempting to pass generic type parameters for a symbol that does not take any.", gen_sym);
            continue;
        }

        if(!postparse_reparse_procedure_permutation(
            gen_base,
            gen_sym,
            parser,
            lxr)
        ) {
            return false;
        }
    }

    errs.emit();
    return !errs.failed();
}