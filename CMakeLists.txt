cmake_minimum_required(VERSION 3.29)
set(CMAKE_PROJECT_TOP_LEVEL_INCLUDES conan_provider.cmake)
project(jaydk)

set(CMAKE_CXX_STANDARD 20)

find_package(argparse REQUIRED)
find_package(termcolor REQUIRED)

add_subdirectory(common/)
add_subdirectory(jayc/)
add_subdirectory(jayvm/)