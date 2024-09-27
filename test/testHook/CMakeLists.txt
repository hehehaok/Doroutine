add_executable(testHook)

target_include_directories(testHook PRIVATE ${INCLUDE})

file(GLOB MAIN_SRC ${SRC}/*.cpp)

target_sources(testHook PRIVATE testHook.cpp ${MAIN_SRC})

force_redefine_file_macro_for_sources(testHook)