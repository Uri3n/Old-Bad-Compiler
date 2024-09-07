cmake_minimum_required(VERSION 3.28)
project(tak)

set(CMAKE_CXX_STANDARD 20)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Located LLVM: ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")


add_executable(tak src/main.cpp
        src/lexer/iterate.cpp
        src/lexer/tokens.cpp
        src/lexer/errors.cpp
        src/lexer/init.cpp
        src/parser/begin.cpp
        src/parser/declarations.cpp
        src/parser/expressions.cpp
        src/parser/controlflow.cpp
        src/parser/type.cpp
        src/parser/namespace.cpp
        src/parser/ident.cpp
        src/parser/directive.cpp
        src/parser/enumdef.cpp
        src/parser/structdef.cpp
        src/parser/dump_nodes.cpp
        src/checker/convert.cpp
        src/checker/typedata.cpp
        src/checker/evaluate.cpp
        src/entity_table/types.cpp
        src/entity_table/symbols.cpp
        src/entity_table/namespaces.cpp
        src/entity_table/scope.cpp
        src/entity_table/dump_entities.cpp
        src/support/util.cpp
        src/support/typedata_util.cpp
        src/support/token_util.cpp
        src/support/destructors.cpp
        src/support/do_compile.cpp
        src/support/semantic_error_handler.cpp
        src/support/io.cpp
        src/support/argparse.cpp
        src/postparser/generic_procedures.cpp
        src/postparser/generic_structs.cpp
        src/postparser/other.cpp
        src/codegen/init.cpp
        src/codegen/expressions.cpp
        src/codegen/declarations.cpp
        src/codegen/controlflow.cpp
        src/codegen/llvm_types.cpp
        src/codegen/begin.cpp
        src/codegen/constants.cpp
        src/codegen/defer.cpp
        src/codegen/context.cpp
        src/codegen/wrapped.cpp
        include/token.hpp
        include/Lexer.hpp
        include/io.hpp
        include/defer.hpp
        include/parser.hpp
        include/ast_types.hpp
        include/typedata.hpp
        include/checker.hpp
        include/panic.hpp
        include/util.hpp
        include/codegen.hpp
        include/semantic_error_handler.hpp
        include/postparser.hpp
        include/entity_table.hpp
        include/argparse.hpp
        include/do_compile.hpp
        include/config.hpp
        src/support/config.cpp
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

llvm_map_components_to_libnames(llvm_libs
    support
    core
    irreader
    Target
    TargetParser
    AsmParser
    AsmPrinter
    AllTargetsAsmParsers
    AllTargetsCodeGens
    AllTargetsDescs
    AllTargetsInfos
)

target_link_libraries(tak ${llvm_libs})