cmake_minimum_required(VERSION 3.28)
project(tak)

set(CMAKE_CXX_STANDARD 20)

add_executable(tak src/main.cpp

        src/lexer/lexer_lex.cpp
        src/lexer/lexer_util.cpp
        src/lexer/lexer_tokens.cpp
        src/lexer/lexer_ambiguous_token.cpp
        src/lexer/lexer_report_error.cpp

        src/parser/parser_begin.cpp
        src/parser/parser_symtbl.cpp
        src/parser/parser_util.cpp
        src/parser/parser_dump_nodes.cpp
        src/parser/parser_parsedecl.cpp
        src/parser/parser_parse_expressions.cpp
        src/parser/parser_parse_keywords.cpp
        src/parser/parser_misc.cpp

        include/token.hpp
        include/lexer.hpp
        include/io.hpp
        include/defer.hpp
        include/parser.hpp
        include/ast_types.hpp
        include/sym_types.hpp
        src/parser/parser_typetbl.cpp
)

target_include_directories(tak PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)