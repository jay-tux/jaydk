cmake_minimum_required(VERSION 3.29)
project(jaydk_common)

set(CMAKE_CXX_STANDARD 20)

add_library(jaydk_common SHARED jaydk.cpp)

target_link_libraries(jaydk_common termcolor::termcolor)