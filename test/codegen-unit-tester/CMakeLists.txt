cmake_minimum_required(VERSION 3.16)

add_executable(codegen-unit-tester
    basicTests.cpp
    ../../src/codegen/PlnRegAlloc.cpp
)
target_include_directories(codegen-unit-tester PRIVATE ../../src/codegen)
target_link_libraries(codegen-unit-tester PRIVATE gtest_main)

include(GoogleTest)
gtest_discover_tests(codegen-unit-tester)
