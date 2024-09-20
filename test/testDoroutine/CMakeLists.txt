add_executable(testDoroutine)

target_include_directories(testDoroutine PRIVATE ${INCLUDE})

target_sources(testDoroutine PRIVATE testDoroutine.cpp ${SRC}/doroutine.cpp)