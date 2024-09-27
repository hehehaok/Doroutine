add_executable(testLog)

target_include_directories(testLog PRIVATE ${INCLUDE})

file(GLOB MAIN_SRC ${SRC}/*.cpp)

target_sources(testLog PRIVATE testLog.cpp ${MAIN_SRC})

force_redefine_file_macro_for_sources(testLog)