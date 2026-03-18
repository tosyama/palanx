cmake_minimum_required(VERSION 3.16)

enable_testing()

add_executable(codegen-tester basicTests.cpp
    ../../src/codegen/PlnRegAlloc.cpp)
target_link_libraries(codegen-tester PRIVATE gtest_main testbase)

include(GoogleTest)
