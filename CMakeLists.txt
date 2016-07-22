cmake_minimum_required(VERSION 2.8)
project(stepic_cpp_parallel_final)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -lrt -pthread")

set(SOURCE_FILES main.cpp)
add_executable(final ${SOURCE_FILES})