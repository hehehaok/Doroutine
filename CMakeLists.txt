cmake_minimum_required(VERSION 3.22.0)

project(DoroutineFromScratch)

set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-builtin-macro-redefined")

set(CMAKE_GENERATOR "Unix Makefiles")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(INCLUDE ${PROJECT_SOURCE_DIR}/include)
set(SRC ${PROJECT_SOURCE_DIR}/src)

function(force_redefine_file_macro_for_sources targetname)
    get_target_property(source_files "${targetname}" SOURCES)
    foreach(sourcefile ${source_files})
        # Get source file's current list of compile definitions.
        get_property(defs SOURCE "${sourcefile}"
            PROPERTY COMPILE_DEFINITIONS)
        # Get the relative path of the source file in project directory
        get_filename_component(filepath "${sourcefile}" ABSOLUTE)
        string(REPLACE ${PROJECT_SOURCE_DIR}/ "" relpath ${filepath})
        list(APPEND defs "__FILE__=\"${relpath}\"")
        # Set the updated compile definitions on the source file.
        set_property(
            SOURCE "${sourcefile}"
            PROPERTY COMPILE_DEFINITIONS ${defs}
            )
    endforeach()
endfunction()

add_subdirectory(test/testLog)
add_subdirectory(test/testDoroutine)
add_subdirectory(test/testScheduler)
add_subdirectory(test/testTimer)
add_subdirectory(test/testIOManager)
add_subdirectory(test/testHook)