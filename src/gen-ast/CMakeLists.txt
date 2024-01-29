cmake_minimum_required(VERSION 3.16)

find_package(FLEX)
FLEX_TARGET(PlnLexer PlnLexer.ll ${CMAKE_CURRENT_SOURCE_DIR}/PlnLexer.cpp)

add_executable(gen-ast
	main.cpp
	PlnGenAstMessage.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/PlnLexer.cpp
)
