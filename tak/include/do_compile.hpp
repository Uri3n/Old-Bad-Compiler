//
// Created by Diago on 2024-09-03.
//

#ifndef DO_COMPILE_HPP
#define DO_COMPILE_HPP
#include <io.hpp>
#include <lexer.hpp>
#include <parser.hpp>
#include <checker.hpp>
#include <postparser.hpp>
#include <codegen.hpp>

namespace tak {
    bool do_compile(const std::string& source_file_name);
    bool do_create_ast(Parser& parser, const std::string& source_file_name);
    void do_codegen(Parser& parser, const std::string& llvm_mod_name);
    bool do_check(Parser& parser, const std::string& original_file_name);
    bool do_parse(Parser& parser, Lexer& lexer);
    bool get_next_include(Parser& parser, Lexer& lexer);
}

#endif //DO_COMPILE_HPP
