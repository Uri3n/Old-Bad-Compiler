//
// Created by Diago on 2024-07-22.
//

#include <checker.hpp>

void
checker_context::raise_error(const std::string& message, const size_t position) {
    lxr_.raise_error(fmt("ERROR: {}", message), position);
    ++error_count_;

    if(error_count_ >= MAX_ERROR_COUNT) {
        panic("\nMaximum error count reached. Aborting compilation.");
    }
}

void
checker_context::raise_warning(const std::string& message, const size_t position) {
    lxr_.raise_error(fmt("WARNING: {}", message), position);
    ++warning_count_;
}