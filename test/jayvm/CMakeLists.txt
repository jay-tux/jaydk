cmake_minimum_required(VERSION 3.29)
project(jayvm_test)

set(CMAKE_CXX_STANDARD 20)

add_executable(jayvm_test main.cpp)

target_link_libraries(jayvm_test jaydk_common)
target_link_libraries(jayvm_test jayvm_lib)
target_link_libraries(jayvm_test doctest::doctest)

target_include_directories(jayvm_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(jayvm_test PUBLIC ${CMAKE_SOURCE_DIR}/common)
target_include_directories(jayvm_test PUBLIC ${CMAKE_SOURCE_DIR}/jayvm)