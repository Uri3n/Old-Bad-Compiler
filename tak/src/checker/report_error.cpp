//
// Created by Diago on 2024-07-22.
//

#include <checker.hpp>

void
CheckerContext::raise_error(const std::string& message, const size_t position) {
    lxr_.raise_error(fmt("ERROR: {}\n", message), position);
    ++error_count_;

    if(error_count_ >= MAX_ERROR_COUNT) {
        panic("\nMaximum error count reached. Aborting compilation.");
    }
}

void
CheckerContext::raise_warning(const std::string& message, const size_t position) {
    lxr_.raise_error(fmt("WARNING: {}\n", message), position);
    ++warning_count_;
}