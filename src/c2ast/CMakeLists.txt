cmake_minimum_required(VERSION 3.16)

add_executable(palan-c2ast
	main.cpp
	PlnC2AstMessage.cpp
	CFileInfo.cpp
	CToken.cpp
	CLexer.cpp
	CPreprocessor.cpp
	CParser.cpp
)

target_compile_options(palan-c2ast PRIVATE -Wno-multichar)

option(OUTPUT_COVERAGE "Output coverage files.")

if (CMAKE_COMPILER_IS_GNUCXX)
	if (OUTPUT_COVERAGE)
		message("Coverage: ON")
		target_compile_options(palan-c2ast PRIVATE --coverage)
		set_target_properties(palan-c2ast PROPERTIES LINK_FLAGS "--coverage")
	endif()
endif()

file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/c2ast)
file(COPY_FILE predefined.h ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/c2ast/predefined.h)

