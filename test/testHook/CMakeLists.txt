add_executable(testHook)

target_include_directories(testHook PRIVATE ${INCLUDE})

file(GLOB MAIN_SRC ${SRC}/*.cpp)

target_sources(testHook PRIVATE testHook.cpp ${MAIN_SRC})