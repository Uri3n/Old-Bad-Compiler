//
// Created by Diago on 2024-08-11.
//

#ifndef SEMANTIC_ERROR_HANDLER_HPP
#define SEMANTIC_ERROR_HANDLER_HPP
#include <lexer.hpp>
#include <ast_types.hpp>
#include <entity_table.hpp>
#include <map>
#include <io.hpp>

#define MAX_ERROR_COUNT 35

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace tak {
    class SemanticErrorHandler {
    public:

        struct ErrorType {
            std::string message;
            size_t   src_index = 0;
            uint32_t line      = 1;
            bool     warning   = false;
        };

        std::map<std::string, std::vector<ErrorType>> errors_;
        uint8_t  error_count_   = 0;
        uint32_t warning_count_ = 0;
        
        bool failed();
        void emit();
        void _max_err_chk();

        void raise_error(const std::string& message, const std::string& file, size_t position, uint32_t line);
        void raise_error(const std::string& message, const AstNode* node);
        void raise_error(const std::string& message, const Symbol* sym);

        void raise_warning(const std::string& message, const std::string& file, size_t position, uint32_t line);
        void raise_warning(const std::string& message, const AstNode* node);
        void raise_warning(const std::string& message, const Symbol* sym);

        SemanticErrorHandler()  = default;
        ~SemanticErrorHandler() = default;
    };
}

#endif //SEMANTIC_ERROR_HANDLER_HPP
