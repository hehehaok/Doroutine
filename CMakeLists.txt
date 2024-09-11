cmake_minimum_required(VERSION 3.22.0)

project(test)

set(CMAKE_CXX_STANDARD 11)


set(CMAKE_GENERATOR "Unix Makefiles")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

add_executable(test main.cpp doroutine.cpp doroutine.h)
