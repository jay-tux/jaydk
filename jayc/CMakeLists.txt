cmake_minimum_required(VERSION 3.29)
project(jayc)

set(CMAKE_CXX_STANDARD 20)

add_library(jayc_lib STATIC jayc.cpp
        error_queue.cpp
        lexer/token_stream.cpp lexer/lexer.cpp lexer/lex_fsm.cpp
)
add_executable(jayc main.cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fconcepts-diagnostics-depth=5")

target_link_libraries(jayc_lib jaydk_common termcolor::termcolor)
target_link_libraries(jayc jayc_lib)
target_link_libraries(jayc argparse::argparse)

target_include_directories(jayc_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/common)