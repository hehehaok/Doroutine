add_executable(coroutineBenchmark)

target_include_directories(coroutineBenchmark PRIVATE ${INCLUDE})

file(GLOB MAIN_SRC ${SRC}/*.cpp)

target_sources(coroutineBenchmark PRIVATE coroutineBenchmark.cpp ${MAIN_SRC})

force_redefine_file_macro_for_sources(coroutineBenchmark)