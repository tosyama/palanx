cmake_minimum_required(VERSION 3.16)

enable_testing()

add_executable(sa-tester basicTests.cpp errorTests.cpp)
target_link_libraries(sa-tester PRIVATE gtest_main testbase)

include(GoogleTest)
