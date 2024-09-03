cmake_minimum_required(VERSION 3.29)
project(jaydk_common_test)

set(CMAKE_CXX_STANDARD 20)

add_executable(jaydk_common_test main.cpp)

target_link_libraries(jaydk_common_test jaydk_common)
target_link_libraries(jaydk_common_test doctest::doctest)

target_include_directories(jaydk_common_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(jaydk_common_test PUBLIC ${CMAKE_SOURCE_DIR}/common)