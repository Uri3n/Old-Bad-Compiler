cmake_minimum_required(VERSION 3.28)
project(tak)

set(CMAKE_CXX_STANDARD 20)

add_executable(tak src/main.cpp

        src/lexer/lex.cpp
        src/lexer/util.cpp
        src/lexer/tokens.cpp
        src/lexer/ambiguous_token.cpp
        src/lexer/report_error.cpp

        src/parser/begin.cpp
        src/parser/symtbl.cpp
        src/parser/util.cpp
        src/parser/dump.cpp
        src/parser/decl.cpp
        src/parser/expr.cpp
        src/parser/ctrlflow.cpp
        src/parser/type.cpp
        src/parser/misc.cpp
        src/parser/typetbl.cpp
        src/parser/namespaces.cpp
        src/parser/ident.cpp
        src/parser/directive.cpp
        src/parser/enumdef.cpp
        src/parser/structdef.cpp

        include/token.hpp
        include/lexer.hpp
        include/io.hpp
        include/defer.hpp
        include/parser.hpp
        include/ast_types.hpp
        include/sym_types.hpp
)

target_include_directories(tak PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)