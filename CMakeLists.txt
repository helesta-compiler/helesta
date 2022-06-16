project(helesta CXX)
cmake_minimum_required(VERSION 3.16)

if(" ${CMAKE_SOURCE_DIR}" STREQUAL " ${CMAKE_BINARY_DIR}")
	message(FATAL_ERROR "
FATAL: In-source builds are not allowed.
	You should create a separete directory for build files.
")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#aux_source_directory(. DIR_SRC)
file(GLOB_RECURSE DIR_SRC "src/*.cpp")
file(GLOB_RECURSE DIR_LIB_SRC "third_party/*.cpp")
add_compile_options(-g -std=c++17 -O2)
include_directories(src)
include_directories(third_party/antlr4-runtime)
add_executable(compiler ${DIR_SRC} ${DIR_LIB_SRC})


include(formatting.cmake)
clang_format(format ${DIR_SRC})
clang_format_check(format_check ${DIR_SRC})
