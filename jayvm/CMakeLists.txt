cmake_minimum_required(VERSION 3.29)
project(jayc)

set(CMAKE_CXX_STANDARD 20)

add_library(jayvm_lib STATIC jayvm.cpp)
add_executable(jayvm main.cpp)

target_link_libraries(jayvm jayvm_lib jaydk_common)
target_link_libraries(jayvm argparse::argparse)