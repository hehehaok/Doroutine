add_executable(testScheduler)

target_include_directories(testScheduler PRIVATE ${INCLUDE})

file(GLOB MAIN_SRC ${SRC}/*.cpp)

target_sources(testScheduler PRIVATE testScheduler.cpp ${MAIN_SRC})

force_redefine_file_macro_for_sources(testScheduler)