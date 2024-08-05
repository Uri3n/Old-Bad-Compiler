cmake_minimum_required(VERSION 3.28)
project(tak)

set(CMAKE_CXX_STANDARD 20)

add_executable(tak src/main.cpp

        src/lexer/lex.cpp
        src/lexer/iterate.cpp
        src/lexer/tokens.cpp
        src/lexer/ambiguous_token.cpp
        src/lexer/report_error.cpp
        src/lexer/init.cpp

        src/parser/symtbl.cpp
        src/parser/dump.cpp
        src/parser/decl.cpp
        src/parser/expr.cpp
        src/parser/ctrlflow.cpp
        src/parser/type.cpp
        src/parser/typetbl.cpp
        src/parser/namespaces.cpp
        src/parser/ident.cpp
        src/parser/directive.cpp
        src/parser/enumdef.cpp
        src/parser/structdef.cpp
        src/parser/compose.cpp

        src/checker/report_error.cpp
        src/checker/convert.cpp
        src/checker/verify.cpp
        src/checker/visit.cpp

        src/support/basic_utility.cpp
        src/support/destructors.cpp
        src/support/do_compile.cpp

        include/token.hpp
        include/Lexer.hpp
        include/io.hpp
        include/defer.hpp
        include/parser.hpp
        include/ast_types.hpp
        include/var_types.hpp
        include/checker.hpp
        include/panic.hpp
        include/support.hpp
        src/support/io.cpp
)

if(WIN32)
    target_compile_definitions(tak PRIVATE TAK_WINDOWS)
else()
    target_compile_definitions(tak PRIVATE TAK_UNIX)
endif()

target_compile_definitions(tak PRIVATE TAK_DEBUG)
target_include_directories(tak PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)