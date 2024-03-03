cmake_minimum_required(VERSION 3.16)

enable_testing()

add_executable(build-mgr-tester basicTests.cpp)
target_link_libraries(build-mgr-tester PRIVATE gtest_main testbase)

include(GoogleTest)
