cmake_minimum_required(VERSION 3.16)

enable_testing()

add_executable(gen-ast-tester basicTests.cpp)
target_link_libraries(gen-ast-tester PRIVATE gtest_main testbase)

include(GoogleTest)
