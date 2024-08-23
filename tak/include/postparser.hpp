//
// Created by Diago on 2024-08-11.
//

#ifndef POSTPARSER_HPP
#define POSTPARSER_HPP
#include <parser.hpp>
#include <cstdint>
#include <semantic_error_handler.hpp>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace tak {

    struct LocationType {
        std::string file;
        size_t pos;
        uint32_t line;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void postparse_inspect_struct_t(Parser& parser, TypeData& type, SemanticErrorHandler& errs, const LocationType& loc);
    void postparse_inspect_proc_t(Parser& parser, const TypeData& type, SemanticErrorHandler& errs, const LocationType& loc);
    bool postparse_verify(Parser& parser, Lexer& lxr);
    bool postparse_check_leftover_placeholders(Parser& parser);
    bool postparse_check_recursive_structures(Parser& parser);
    bool postparse_permute_generic_structures(Parser& parser);
    bool postparse_permute_generic_procedures(Parser& parser, Lexer& lxr);
    bool postparse_reparse_procedure_permutation(const Symbol* old, Symbol* perm, Parser& parser, Lexer& lxr);
    bool postparse_delete_garbage_objects(Parser& parser);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    std::optional<TypeData> postparse_try_permute_member(
        const std::unordered_map<std::string,TypeData*>& gen_map,
        const TypeData& old_member,
        Parser& parser,
        SemanticErrorHandler& errs,
        const LocationType& loc
    );

    bool postparse_try_create_permutation(
        const std::string& name,
        Parser& parser,
        TypeData& to_conv,
        const UserType* old_t,
        SemanticErrorHandler& errs,
        const LocationType& loc
    );
}

#endif //POSTPARSER_HPP
