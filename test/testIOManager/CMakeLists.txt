add_executable(testIOManager)

target_include_directories(testIOManager PRIVATE ${INCLUDE})

file(GLOB MAIN_SRC ${SRC}/*.cpp)

target_sources(testIOManager PRIVATE testIOManager.cpp ${MAIN_SRC})