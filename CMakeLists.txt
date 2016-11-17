cmake_minimum_required (VERSION 2.6)
project (Filestreamer)

SET(CMAKE_BUILD_TYPE "Debug")

include_directories("${PROJECT_SOURCE_DIR}/src/include/")

SET(CMAKE_C_FLAGS "-std=gnu99 -O3")
SET(CMAKE_C_FLAGS_DEBUG "-std=gnu99 -g -Wall -Werror")

SET(CMAKE_CXX_FLAGS "-fpermissive -O3")
SET(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Werror")

include_directories("lib/libneat")
link_directories("${PROJECT_SOURCE_DIR}/lib/libneat")

add_executable(stream_socket
    "src/file_reader.c"
    "src/server_sockets.c"
    "src/main.c"
    )

add_executable(stream_neat
    "src/file_reader.c"
    "src/server_neat.c"
    "src/main.c"
    )

target_compile_definitions(stream_neat PRIVATE WITH_NEAT)
target_link_libraries(stream_neat libneat.so)

enable_testing()
add_subdirectory(test)
