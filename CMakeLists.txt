project(helesta CXX)
cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_COMPILER "clang++")

if(" ${CMAKE_SOURCE_DIR}" STREQUAL " ${CMAKE_BINARY_DIR}")
	message(FATAL_ERROR "
FATAL: In-source builds are not allowed.
	You should create a separete directory for build files.
")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#aux_source_directory(. DIR_SRC)
file(GLOB_RECURSE DIR_SRC "src/arm/*.cpp" "src/ast/*.cpp" "src/ir/*.cpp" "src/common/*.cpp" "src/main.cpp")
file(GLOB_RECURSE PSR_SRC "src/parser/*.cpp")
file(GLOB_RECURSE HDR_SRC "src/*.hpp")
file(GLOB_RECURSE DIR_LIB_SRC "third_party/*.cpp")
add_compile_options(-g3 -std=c++17 -O2 -Wno-attributes)
include_directories(src)
include_directories(third_party/antlr4-runtime)
set_property(SOURCE ${DIR_SRC} PROPERTY COMPILE_FLAGS "-Wall -Wextra -Werror -Wno-error=attributes")
add_executable(compiler ${DIR_SRC} ${PSR_SRC} ${DIR_LIB_SRC})

include(formatting.cmake)
clang_format(format ${DIR_SRC} ${HDR_SRC})
clang_format_check(format_check ${DIR_SRC} ${HDR_SRC})
