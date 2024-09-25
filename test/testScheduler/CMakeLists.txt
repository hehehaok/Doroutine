add_executable(testScheduler)

target_include_directories(testScheduler PRIVATE ${INCLUDE})

file(GLOB MAIN_SRC ${SRC}/*.cpp)

target_sources(testScheduler PRIVATE testScheduler.cpp ${MAIN_SRC})