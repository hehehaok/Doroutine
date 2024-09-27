add_executable(testTimer)

target_include_directories(testTimer PRIVATE ${INCLUDE})

file(GLOB MAIN_SRC ${SRC}/*.cpp)

target_sources(testTimer PRIVATE testTimer.cpp ${MAIN_SRC})

force_redefine_file_macro_for_sources(testTimer)