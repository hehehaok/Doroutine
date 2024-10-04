add_executable(libeventBenchmark)

target_sources(libeventBenchmark PRIVATE libeventBenchmark.cpp)

target_link_libraries(libeventBenchmark PRIVATE event)


