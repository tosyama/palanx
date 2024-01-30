cmake_minimum_required(VERSION 3.16)

find_package(FLEX)
FLEX_TARGET(PlnLexer ${CMAKE_CURRENT_SOURCE_DIR}/PlnLexer.ll ${CMAKE_CURRENT_SOURCE_DIR}/PlnLexer.cpp)

add_executable(gen-ast
	main.cpp
	PlnGenAstMessage.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/PlnLexer.cpp
)

option(OUTPUT_COVERAGE "Output coverage files.")

if (CMAKE_COMPILER_IS_GNUCXX)
	if (OUTPUT_COVERAGE)
		message("Coverage: ON")
		target_compile_options(gen-ast PRIVATE --coverage)
		set_target_properties(gen-ast PROPERTIES LINK_FLAGS "--coverage")
	endif()
endif()

