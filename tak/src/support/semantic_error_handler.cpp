//
// Created by Diago on 2024-08-11.
//

#include <semantic_error_handler.hpp>

void
tak::SemanticErrorHandler::_max_err_chk() {
    if(error_count_ + 1 >= MAX_ERROR_COUNT) {
        emit();
        red_bold("\nMaximum amount of permitted errors ({}) reached. Compilation was aborted.", MAX_ERROR_COUNT);
        exit(1);
    }
}

void
tak::SemanticErrorHandler::raise_error(const std::string& message, const std::string& file, const size_t position, const uint32_t line) {
    assert(position);
    assert(line);
    assert(!file.empty());

    _max_err_chk();
    ++error_count_;
    errors_[file].emplace_back(ErrorType{ message, position, line, false });
}

void
tak::SemanticErrorHandler::raise_error(const std::string& message, const AstNode* node) {
    assert(node != nullptr);
    assert(node->src_pos);
    assert(node->line);
    assert(!node->file.empty());

    _max_err_chk();
    ++error_count_;
    errors_[node->file].emplace_back(ErrorType{ message, node->src_pos, node->line, false });
}

void
tak::SemanticErrorHandler::raise_error(const std::string& message, const Symbol* sym) {
    assert(sym != nullptr);
    assert(sym->src_pos);
    assert(sym->line_number);
    assert(!sym->file.empty());

    _max_err_chk();
    ++error_count_;
    errors_[sym->file].emplace_back(ErrorType{ message, sym->src_pos, sym->line_number, false });
}

void
tak::SemanticErrorHandler::raise_warning(const std::string& message, const std::string& file, const size_t position, const uint32_t line) {
    if(Config::get().flags() & CONFIG_WARN_IS_ERR) {
        raise_error(message, file, position, line);
        return;
    }

    ++warning_count_;
    errors_[file].emplace_back(ErrorType{ message, position, line, true });
}

void
tak::SemanticErrorHandler::raise_warning(const std::string& message, const AstNode* node) {
    assert(node != nullptr);
    ++warning_count_;
    errors_[node->file].emplace_back(ErrorType{ message, node->src_pos, node->line, true });
}

void
tak::SemanticErrorHandler::raise_warning(const std::string& message, const Symbol* sym) {
    assert(sym != nullptr);
    ++warning_count_;
    errors_[sym->file].emplace_back(ErrorType{ message, sym->src_pos, sym->line_number, true });
}

void
tak::SemanticErrorHandler::emit() {

    Lexer lxr;
    for(const auto &[file_name, file_errors] : errors_) {
        assert(lxr.init(file_name));
        for(const auto& error : file_errors) {
            if(error.warning) lxr.raise_warning(fmt("WARNING: {}", error.message), error.src_index, error.line);
            else              lxr.raise_error(fmt("ERROR: {}", error.message), error.src_index, error.line);
        }
    }
}

bool
tak::SemanticErrorHandler::failed() {
    return error_count_ > 0;
}
