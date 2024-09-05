cmake_minimum_required(VERSION 3.16)

add_executable(palan-c2ast
	main.cpp
	cfileinfo.cpp
	ctoken.cpp
	clexer.cpp
	cpreprocessor.cpp
)

file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/c2ast)
file(COPY predefined.h DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/c2ast)
