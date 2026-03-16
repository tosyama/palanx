cmake_minimum_required(VERSION 3.16)

add_executable(sa-unit-tester
    basicTests.cpp
    ../../src/semantic-anlyzr/PlnType.cpp
)
target_include_directories(sa-unit-tester PRIVATE ../../src/semantic-anlyzr ../../lib/json/single_include)
target_link_libraries(sa-unit-tester PRIVATE gtest_main)

include(GoogleTest)
gtest_discover_tests(sa-unit-tester)
