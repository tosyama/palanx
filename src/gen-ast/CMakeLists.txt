cmake_minimum_required(VERSION 3.16)

find_package(FLEX)
find_package(BISON 3.8)

flex_target(PlnLexer ${CMAKE_CURRENT_SOURCE_DIR}/PlnLexer.ll ${CMAKE_CURRENT_SOURCE_DIR}/PlnLexer.cpp)
bison_target(PlnParser ${CMAKE_CURRENT_SOURCE_DIR}/PlnParser.yy ${CMAKE_CURRENT_SOURCE_DIR}/PlnParser.cpp DEFINES_FILE ${CMAKE_CURRENT_SOURCE_DIR}/PlnParser.h)
#bison_target(PlnParser ${CMAKE_CURRENT_SOURCE_DIR}/PlnParser.yy ${CMAKE_CURRENT_SOURCE_DIR}/PlnParser.cpp DEFINES_FILE ${CMAKE_CURRENT_SOURCE_DIR}/PlnParser.h COMPILE_FLAGS -Wcex)
add_flex_bison_dependency(PlnLexer PlnParser)

add_executable(palan-gen-ast
	main.cpp
	PlnGenAstMessage.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/PlnLexer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/PlnParser.cpp
)

option(OUTPUT_COVERAGE "Output coverage files.")

if (CMAKE_COMPILER_IS_GNUCXX)
	if (OUTPUT_COVERAGE)
		message("Coverage: ON")
		target_compile_options(palan-gen-ast PRIVATE --coverage)
		set_target_properties(palan-gen-ast PROPERTIES LINK_FLAGS "--coverage")
	endif()
endif()

