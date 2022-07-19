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
add_compile_options(-g3 -std=c++17 -O0 -Wno-attributes)
include_directories(src)
include_directories(third_party/antlr4-runtime)
set_property(SOURCE ${DIR_SRC} PROPERTY COMPILE_FLAGS "-Wall -Wextra -Werror -Wno-error=attributes")
add_executable(compiler ${DIR_SRC} ${PSR_SRC} ${DIR_LIB_SRC})

include(formatting.cmake)
clang_format(format ${DIR_SRC} ${HDR_SRC})
clang_format_check(format_check ${DIR_SRC} ${HDR_SRC})

# option for test
# option(CUSTOM_TEST "Run custom test cases" ON)
option(FUNC_TEST "Run functional test cases" ON)
option(PERF_TEST "Run performance test cases" ON)

# message("Custom test: ${CUSTOM_TEST}")
message("Functional test: ${FUNC_TEST}")
message("Performance test: ${PERF_TEST}")

# add test, usage: `make test` or `ctest`
enable_testing()

# test files
file(GLOB functional_test "testcases/公开样例与运行时库/functional/*.sy")
file(GLOB performance_test "testcases/公开样例与运行时库/performance/*.sy")
# file(GLOB custom_test_cases "test_sets/*.sy")

# output .s file path
set(output_file_path "${CMAKE_BINARY_DIR}/output")
file(MAKE_DIRECTORY "${output_file_path}")

# test which cases
set(all_test_cases "")
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
# if (CUSTOM_TEST)
#	set(all_test_cases ${all_test_cases} ${custom_test_cases})
# endif()

# test start, officially

enable_testing()