cmake_minimum_required(VERSION 3.20)
project(MiniCppCompiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(BISON 3.0 QUIET)
find_package(FLEX 2.6 QUIET)

set(MINICPP_PARSER_SOURCES)
if(BISON_FOUND AND FLEX_FOUND)
    message(STATUS "Flex/Bison found: using the LALR(1) parser generated from lexer.l and parser.y")
    BISON_TARGET(MiniCppParser
        ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.y
        ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.cpp
        DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.h
    )
    FLEX_TARGET(MiniCppLexer
        ${CMAKE_CURRENT_SOURCE_DIR}/src/lexer.l
        ${CMAKE_CURRENT_BINARY_DIR}/lexer.yy.cpp
    )
    ADD_FLEX_BISON_DEPENDENCY(MiniCppLexer MiniCppParser)
    list(APPEND MINICPP_PARSER_SOURCES ${BISON_MiniCppParser_OUTPUTS} ${FLEX_MiniCppLexer_OUTPUTS})
else()
    message(WARNING "Flex/Bison not found: building the equivalent recursive-descent fallback parser. Install flex and bison to use the requested LALR(1) Flex/Bison implementation.")
    list(APPEND MINICPP_PARSER_SOURCES src/fallback_parser.cpp)
endif()

add_executable(minicpp
    src/main.cpp
    src/ast.cpp
    src/semantic.cpp
    src/interpreter.cpp
    src/codegen.cpp
    src/pipeline.cpp
    src/server.cpp
    ${MINICPP_PARSER_SOURCES}
)

target_include_directories(minicpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(minicpp PRIVATE MINICPP_WEB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/web")
if(NOT (BISON_FOUND AND FLEX_FOUND))
    target_compile_definitions(minicpp PRIVATE MINICPP_FALLBACK_PARSER=1)
endif()

if(WIN32)
    target_link_libraries(minicpp PRIVATE ws2_32)
endif()

if(MSVC)
    target_compile_options(minicpp PRIVATE /W4)
else()
    target_compile_options(minicpp PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS minicpp RUNTIME DESTINATION bin)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME demo_run COMMAND minicpp ${CMAKE_CURRENT_SOURCE_DIR}/examples/demo.cpp --run)
    set_tests_properties(demo_run PROPERTIES PASS_REGULAR_EXPRESSION "Program returned: 120")
    add_test(NAME functions_run COMMAND minicpp ${CMAKE_CURRENT_SOURCE_DIR}/examples/functions.cpp --run)
    set_tests_properties(functions_run PROPERTIES PASS_REGULAR_EXPRESSION "Program returned: 1")
    add_test(NAME while_run COMMAND minicpp ${CMAKE_CURRENT_SOURCE_DIR}/examples/while.cpp --run)
    set_tests_properties(while_run PROPERTIES PASS_REGULAR_EXPRESSION "Program returned: 4")
    add_test(NAME void_function_run COMMAND minicpp ${CMAKE_CURRENT_SOURCE_DIR}/examples/void.cpp --run)
    set_tests_properties(void_function_run PROPERTIES PASS_REGULAR_EXPRESSION "Program returned: 10")
    add_test(NAME codegen_edge_cases_run COMMAND minicpp ${CMAKE_CURRENT_SOURCE_DIR}/examples/codegen_edge_cases.cpp --run)
    set_tests_properties(codegen_edge_cases_run PROPERTIES PASS_REGULAR_EXPRESSION "Program returned: 42")
    add_test(NAME syntax_error COMMAND minicpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/syntax_error.cpp --check)
    set_tests_properties(syntax_error PROPERTIES WILL_FAIL TRUE)
    add_test(NAME semantic_error COMMAND minicpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/semantic_error.cpp --check)
    set_tests_properties(semantic_error PROPERTIES WILL_FAIL TRUE)
endif()