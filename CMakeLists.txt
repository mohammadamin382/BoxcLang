

cmake_minimum_required(VERSION 3.10)
project(Box VERSION 0.1.0 LANGUAGES C CXX)
include_directories(${PROJECT_SOURCE_DIR}/src/compiler)
include_directories(${PROJECT_SOURCE_DIR}/src/optimizer)
include_directories(${PROJECT_SOURCE_DIR}/src/parser)
include_directories(${PROJECT_SOURCE_DIR}/src/codegen)
include_directories(${PROJECT_SOURCE_DIR}/src/lexer)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

set(LEXER_SOURCES
    src/lexer/token.cpp
    src/lexer/lexer_error.cpp
    src/lexer/lexer.cpp
)

set(LEXER_HEADERS
    src/lexer/token.hpp
    src/lexer/lexer_error.hpp
    src/lexer/lexer.hpp
)

set(PARSER_SOURCES
    src/parser/parser.cpp
)

set(PARSER_HEADERS
    src/parser/parser.hpp
)

set(MEMORY_ANALYZER_SOURCES
    src/memory_analyzer/memory_analyzer.cpp
)

set(MEMORY_ANALYZER_HEADERS
    src/memory_analyzer/memory_analyzer.hpp
)

set(OPTIMIZER_SOURCES
    src/optimizer/optimizer.cpp
)

set(OPTIMIZER_HEADERS
    src/optimizer/optimizer.hpp
)

set(CMAKE_PREFIX_PATH "$ENV{HOME}/.nix-profile:${CMAKE_PREFIX_PATH}")

find_package(ZLIB QUIET)
if(NOT ZLIB_FOUND)
    add_library(ZLIB::ZLIB INTERFACE IMPORTED)
    set_target_properties(ZLIB::ZLIB PROPERTIES
        INTERFACE_LINK_LIBRARIES "z"
    )
endif()

find_package(Terminfo QUIET)
if(NOT Terminfo_FOUND AND NOT TARGET Terminfo::terminfo)
    add_library(Terminfo::terminfo INTERFACE IMPORTED)
    set_target_properties(Terminfo::terminfo PROPERTIES
        INTERFACE_LINK_LIBRARIES "ncurses"
    )
endif()

find_package(LibXml2 QUIET)
if(NOT LibXml2_FOUND AND NOT TARGET LibXml2::LibXml2)
    add_library(LibXml2::LibXml2 INTERFACE IMPORTED)
    set_target_properties(LibXml2::LibXml2 PROPERTIES
        INTERFACE_LINK_LIBRARIES "xml2"
    )
endif()

find_package(FFI QUIET)
if(NOT FFI_FOUND AND NOT TARGET FFI::ffi)
    add_library(FFI::ffi INTERFACE IMPORTED)
    set_target_properties(FFI::ffi PROPERTIES
        INTERFACE_LINK_LIBRARIES "ffi"
    )
endif()

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

if(LLVM_PACKAGE_VERSION VERSION_LESS "17.0")
    message(WARNING "LLVM version ${LLVM_PACKAGE_VERSION} detected. Version 17+ recommended.")
endif()

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

set(CODEGEN_SOURCES
    src/codegen/box_value.cpp
    src/codegen/environment.cpp
    src/codegen/codegen_error.cpp
    src/codegen/codegen.cpp
    src/codegen/statement_visitors.cpp
    src/codegen/expression_visitors.cpp
    src/codegen/arrays_and_dicts.cpp
    src/codegen/dict_operations.cpp
    src/codegen/visit_switch.cpp
    src/codegen/visit_call.cpp
    src/codegen/builtin_functions.cpp
    src/codegen/llvm_inline.cpp
    src/codegen/utility_functions.cpp
)

set(CODEGEN_HEADERS
    src/codegen/box_value.hpp
    src/codegen/environment.hpp
    src/codegen/codegen_error.hpp
    src/codegen/codegen.hpp
)

add_library(boxlexer STATIC ${LEXER_SOURCES} ${LEXER_HEADERS})
target_include_directories(boxlexer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/lexer)

add_library(boxparser STATIC ${PARSER_SOURCES} ${PARSER_HEADERS})
target_link_libraries(boxparser boxlexer)
target_include_directories(boxparser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/parser)

add_library(boxmemoryanalyzer STATIC ${MEMORY_ANALYZER_SOURCES} ${MEMORY_ANALYZER_HEADERS})
target_link_libraries(boxmemoryanalyzer boxparser boxlexer)
target_include_directories(boxmemoryanalyzer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/memory_analyzer)

add_library(boxoptimizer STATIC ${OPTIMIZER_SOURCES} ${OPTIMIZER_HEADERS})
target_link_libraries(boxoptimizer boxparser boxlexer)
target_include_directories(boxoptimizer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/optimizer)

llvm_map_components_to_libnames(llvm_libs 
    core 
    support 
    irreader
    native
    x86codegen
    x86asmparser
    x86desc
    x86info
    mc
    mcparser
    target
    asmparser
    asmprinter
)

add_library(boxcodegen STATIC ${CODEGEN_SOURCES} ${CODEGEN_HEADERS})
target_link_libraries(boxcodegen boxparser boxlexer boxoptimizer ${llvm_libs})
target_include_directories(boxcodegen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/codegen)

add_library(boxcompiler STATIC
    src/compiler/compiler.cpp
    src/compiler/file_resolver.cpp
)

target_link_libraries(boxcompiler 
    boxlexer
    boxparser
    boxcodegen
    boxmemoryanalyzer
    boxoptimizer
    ${llvm_libs}
)

add_executable(boxc main.cpp)

target_link_libraries(boxc
    boxcompiler
    boxlexer
    boxparser
    boxcodegen
    boxmemoryanalyzer
    boxoptimizer
    ${llvm_libs}
    ncurses
    z
)

file(GLOB LEXER_TESTS "tests/lexer/*_test.cpp")
file(GLOB PARSER_TESTS "tests/parser/*_test.cpp")
file(GLOB MEMORY_TESTS "tests/memory_analyzer/*_test.cpp")
file(GLOB OPTIMIZER_TESTS "tests/optimizer/*_test.cpp")
file(GLOB CODEGEN_TESTS "tests/codegen/*_test.cpp")
file(GLOB COMPILER_TESTS "tests/compiler/*_test.cpp")

foreach(test_file ${LEXER_TESTS})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} boxlexer)
    target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/lexer)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

foreach(test_file ${PARSER_TESTS})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} boxparser boxlexer)
    target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

foreach(test_file ${MEMORY_TESTS})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} boxmemoryanalyzer boxparser boxlexer)
    target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

foreach(test_file ${OPTIMIZER_TESTS})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} boxoptimizer boxparser boxlexer ${llvm_libs})
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

foreach(test_file ${CODEGEN_TESTS})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} boxcodegen boxparser boxlexer boxoptimizer ${llvm_libs})
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

foreach(test_file ${COMPILER_TESTS})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} 
        boxcompiler
        boxlexer
        boxparser
        boxcodegen
        boxmemoryanalyzer
        boxoptimizer
        ${llvm_libs}
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

enable_testing()

install(TARGETS boxc DESTINATION bin)
