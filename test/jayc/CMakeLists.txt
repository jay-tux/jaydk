cmake_minimum_required(VERSION 3.29)
project(jayc_test)

set(CMAKE_CXX_STANDARD 20)

add_executable(jayc_test main.cpp
        lexer_test.cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTEST_SOURCE='\"${CMAKE_CURRENT_SOURCE_DIR}\"'")

target_link_libraries(jayc_test jaydk_common)
target_link_libraries(jayc_test jayc_lib)
target_link_libraries(jayc_test doctest::doctest)

target_include_directories(jayc_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(jayc_test PUBLIC ${CMAKE_SOURCE_DIR}/common)
target_include_directories(jayc_test PUBLIC ${CMAKE_SOURCE_DIR}/jayc)