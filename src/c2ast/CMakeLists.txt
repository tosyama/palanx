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
palan_tool(palan-c2ast)

target_compile_options(palan-c2ast PRIVATE -Wno-multichar)

option(OUTPUT_COVERAGE "Output coverage files.")

if (CMAKE_COMPILER_IS_GNUCXX)
	if (OUTPUT_COVERAGE)
		message("Coverage: ON")
		target_compile_options(palan-c2ast PRIVATE --coverage)
		set_target_properties(palan-c2ast PROPERTIES LINK_FLAGS "--coverage")
	endif()
endif()

configure_file(predefined.h ${PALAN_BUILD_LIB_DIR}/${PALAN_TARGET}/c2ast/predefined.h COPYONLY)
install(FILES predefined.h DESTINATION ${PALAN_INSTALL_LIB_DIR}/${PALAN_TARGET}/c2ast COMPONENT tools)

