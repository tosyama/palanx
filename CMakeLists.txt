cmake_minimum_required(VERSION 3.16)
project(palanx)

include(FetchContent)
FetchContent_Declare(
	googletest
	URL https://github.com/google/googletest/archive/refs/tags/release-1.12.0.zip
	)

FetchContent_MakeAvailable(googletest)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(test/test-base)

add_subdirectory(src/gen-ast)
add_subdirectory(test/gen-ast-tester)
