add_executable(testScheduler)

target_include_directories(testScheduler PRIVATE ${INCLUDE})

target_sources(testScheduler PRIVATE testScheduler.cpp ${SRC}/doroutine.cpp ${SRC}/scheduler.cpp ${SRC}/util.cpp)