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

# option for test
option(CUSTOM_TEST "Run custom test cases" ON)
option(FUNC_TEST "Run functional test cases" OFF)
option(PERF_TEST "Run performance test cases" OFF)

message("Custom test: ${CUSTOM_TEST}")
message("Functional test: ${FUNC_TEST}")
message("Performance test: ${PERF_TEST}")

# add test, usage: `make test` or `ctest`
enable_testing()

# test files
if(NOT CUSTOM_TEST_PATH)
	set(CUSTOM_TEST_PATH "testcases/custom")
endif()
message("Custom test path: ${CUSTOM_TEST_PATH}")
file(GLOB custom_test "${CUSTOM_TEST_PATH}/*.sy")
file(GLOB functional_test "testcases/functional/*.sy")
file(GLOB performance_test "testcases/performance/*.sy")

# output .s file path
set(output_file_path "${CMAKE_BINARY_DIR}/output")
file(MAKE_DIRECTORY "${output_file_path}")

# test which cases
if (CUSTOM_TEST)
	file(MAKE_DIRECTORY "${output_file_path}/custom")
	foreach(file ${custom_test})
		get_filename_component(file_name "${file}" NAME_WE)
		add_test(NAME ${file_name} COMMAND ./compiler "${file}" -S -o "${output_file_path}/custom/${file_name}.s")
	endforeach()
endif()
if (FUNC_TEST)
	file(MAKE_DIRECTORY "${output_file_path}/functional")
	foreach(file ${functional_test})
		get_filename_component(file_name "${file}" NAME_WE)
		add_test(NAME ${file_name} COMMAND ./compiler "${file}" -S -o "${output_file_path}/functional/${file_name}.s")
	endforeach()
endif()
if (PERF_TEST)
	file(MAKE_DIRECTORY "${output_file_path}/performance")
	foreach(file ${performance_test})
		get_filename_component(file_name "${file}" NAME_WE)
		add_test(NAME ${file_name} COMMAND ./compiler "${file}" -S -o "${output_file_path}/performance/${file_name}.s")
	endforeach()
endif()