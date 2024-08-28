//
// Created by Diago on 2024-07-22.
//

#ifndef PANIC_HPP
#define PANIC_HPP
#include <cstdlib>
#include <io.hpp>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

[[noreturn]] inline void
_panic_impl(const std::string& message, const std::string& file, const int64_t line) {
    tak::print<tak::TFG_RED, tak::TBG_NONE, tak::TSTYLE_BOLD>("PANIC :: {}", message);
    tak::print("In file \"{}\" at line {}.", file, line);
    exit(1);
}

#define panic(MSG) ::_panic_impl(MSG, __FILE__, __LINE__)

#endif //PANIC_HPP
