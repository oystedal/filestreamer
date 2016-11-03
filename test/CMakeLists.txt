file(GLOB Tests_SRC "*.cpp")

include_directories("${PROJECT_SOURCE_DIR}/test/lib/Catch/single_include/")
include_directories("${PROJECT_SOURCE_DIR}/src/include/")

CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/mmap_test_input.txt" ${CMAKE_CURRENT_BINARY_DIR}/mmap_test_input.txt COPYONLY)


add_executable(Test ${Tests_SRC} "${PROJECT_SOURCE_DIR}/src/file_reader.c")
add_test(Test Test)
