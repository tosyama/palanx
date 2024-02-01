cmake_minimum_required(VERSION 3.16)

find_package(FLEX)
find_package(BISON)

flex_target(PlnLexer ${CMAKE_CURRENT_SOURCE_DIR}/PlnLexer.ll ${CMAKE_CURRENT_SOURCE_DIR}/PlnLexer.cpp)
bison_target(PlnParser ${CMAKE_CURRENT_SOURCE_DIR}/PlnParser.yy ${CMAKE_CURRENT_SOURCE_DIR}/PlnParser.cpp DEFINES_FILE ${CMAKE_CURRENT_SOURCE_DIR}/PlnParser.h)
add_flex_bison_dependency(PlnLexer PlnParser)

add_executable(gen-ast
	main.cpp
	PlnGenAstMessage.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/PlnLexer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/PlnParser.cpp
)

option(OUTPUT_COVERAGE "Output coverage files.")

if (CMAKE_COMPILER_IS_GNUCXX)
	if (OUTPUT_COVERAGE)
		message("Coverage: ON")
		target_compile_options(gen-ast PRIVATE --coverage)
		set_target_properties(gen-ast PROPERTIES LINK_FLAGS "--coverage")
	endif()
endif()

