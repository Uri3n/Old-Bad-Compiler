cmake_minimum_required(VERSION 3.28)
project(tak)

set(CMAKE_CXX_STANDARD 20)

add_executable(tak src/main.cpp
        include/token.hpp
        src/lexer/lexer_lex.cpp
        include/lexer.hpp
        src/lexer/lexer_util.cpp
        include/io.hpp
        include/defer.hpp
        include/parser.hpp
        include/ast_types.hpp
        src/parser/parser_misc.cpp
        src/lexer/lexer_tokens.cpp
        src/lexer/lexer_ambiguous_token.cpp
        src/lexer/lexer_report_error.cpp
        src/parser/parser.cpp
        src/parser/parser_symtbl.cpp
)

target_include_directories(tak PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)