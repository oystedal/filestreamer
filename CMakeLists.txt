cmake_minimum_required (VERSION 3.0)
project (Filestreamer)

SET(CMAKE_BUILD_TYPE "Debug")

SET(CMAKE_C_FLAGS "-std=c99 -O3")
SET(CMAKE_C_FLAGS_DEBUG "-std=c99 -g -Wall -Werror")

SET(CMAKE_CXX_FLAGS "-fpermissive -O3")
SET(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Werror")

enable_testing()
add_subdirectory(test)
