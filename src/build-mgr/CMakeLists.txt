cmake_minimum_required(VERSION 3.16)

add_executable(palan
	main.cpp
	PlnBuildMgrMessage.cpp
	PlnAllocatorGen.cpp
	../common/PlnProcess.cpp
	../common/PlnInterface.cpp
)
palan_tool(palan)

set(PALAN_LINK_TARGET ../lib/palan/dev/tools/palan)
file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
file(CREATE_LINK ${PALAN_LINK_TARGET} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/palan SYMBOLIC)

# Under DESTDIR too, or a package build would write to the real system.
install(CODE "
	set(link_dir \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/bin\")
	file(MAKE_DIRECTORY \"\${link_dir}\")
	file(CREATE_LINK ../${PALAN_INSTALL_LIB_DIR}/tools/palan \"\${link_dir}/palan\" SYMBOLIC)
" COMPONENT tools)
install(PROGRAMS uninstall.sh DESTINATION ${PALAN_INSTALL_LIB_DIR} RENAME uninstall COMPONENT uninstall)

option(OUTPUT_COVERAGE "Output coverage files.")

if (CMAKE_COMPILER_IS_GNUCXX)
	if (OUTPUT_COVERAGE)
		message("Coverage: ON")
		target_compile_options(palan PRIVATE --coverage)
		set_target_properties(palan PROPERTIES LINK_FLAGS "--coverage")
	endif()
endif()

