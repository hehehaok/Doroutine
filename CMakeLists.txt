cmake_minimum_required(VERSION 3.22.0)

project(DoroutineFromScratch)

set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED 17)

set(CMAKE_GENERATOR "Unix Makefiles")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(INCLUDE ${PROJECT_SOURCE_DIR}/include)
set(SRC ${PROJECT_SOURCE_DIR}/src)

add_subdirectory(test/testDoroutine)
add_subdirectory(test/testScheduler)
add_subdirectory(test/testTimer)
add_subdirectory(test/testIOManager)
