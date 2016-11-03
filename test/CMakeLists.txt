file(GLOB Tests_SRC "*.cpp")

include_directories("${PROJECT_SOURCE_DIR}/test/lib/Catch/single_include/")
include_directories("${PROJECT_SOURCE_DIR}/src/include/")

add_executable(Test ${Tests_SRC})
add_test(Test Test)
