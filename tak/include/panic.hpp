//
// Created by Diago on 2024-07-22.
//

#ifndef PANIC_HPP
#define PANIC_HPP
#include <cstdlib>
#include <io.hpp>
#include <source_location>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

[[noreturn]] inline void
panic(const std::string& message, const std::source_location& loc = std::source_location::current()) {
    tak::print<tak::TFG_RED, tak::TBG_NONE, tak::TSTYLE_BOLD>("PANIC :: {}", message);
    tak::print("In file {} at location {}:{}", loc.file_name(), loc.line(), loc.column());
    exit(1);
}

#endif //PANIC_HPP
