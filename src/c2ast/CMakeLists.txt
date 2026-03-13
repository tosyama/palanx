cmake_minimum_required(VERSION 3.16)

add_executable(palan-c2ast
	main.cpp
	CFileInfo.cpp
	CToken.cpp
	CLexer.cpp
	CPreprocessor.cpp
	CParser.cpp
)

target_compile_options(palan-c2ast PRIVATE -Wno-multichar)

file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/c2ast)
file(COPY_FILE predefined.h ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/c2ast/predefined.h)

