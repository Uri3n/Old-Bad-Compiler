cmake_minimum_required(VERSION 3.28)
project(tak)

set(CMAKE_CXX_STANDARD 20)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Located LLVM: ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")


add_executable(tak src/main.cpp

        src/lexer/lex.cpp
        src/lexer/iterate.cpp
        src/lexer/tokens.cpp
        src/lexer/ambiguous_token.cpp
        src/lexer/errors.cpp
        src/lexer/init.cpp

        src/parser/dump.cpp
        src/parser/decl.cpp
        src/parser/expr.cpp
        src/parser/ctrlflow.cpp
        src/parser/type.cpp
        src/parser/namespace.cpp
        src/parser/ident.cpp
        src/parser/directive.cpp
        src/parser/enumdef.cpp
        src/parser/structdef.cpp

        src/checker/convert.cpp
        src/checker/verify.cpp
        src/checker/visit.cpp

        src/entity_table/types.cpp
        src/entity_table/symbols.cpp
        src/entity_table/namespaces.cpp
        src/entity_table/scope.cpp

        src/support/basic_utility.cpp
        src/support/destructors.cpp
        src/support/do_compile.cpp
        src/support/semantic_error_handler.cpp
        src/support/io.cpp

        src/postparser/generic_procedures.cpp
        src/postparser/generic_structs.cpp
        src/postparser/other.cpp

        src/codegen/codegen.cpp

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
        include/codegen.hpp
        include/semantic_error_handler.hpp
        include/postparser.hpp
        include/entity_table.hpp
)


if(WIN32)
    target_compile_definitions(tak PRIVATE TAK_WINDOWS)
else()
    target_compile_definitions(tak PRIVATE TAK_UNIX)
endif()

target_compile_definitions(tak PRIVATE TAK_DEBUG)
target_include_directories(tak PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(llvm_libs support core irreader)
target_link_libraries(tak ${llvm_libs})