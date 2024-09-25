add_executable(testDoroutine)

target_include_directories(testDoroutine PRIVATE ${INCLUDE})

file(GLOB MAIN_SRC ${SRC}/*.cpp)

target_sources(testDoroutine PRIVATE testDoroutine.cpp ${MAIN_SRC})