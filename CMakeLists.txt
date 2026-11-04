cmake_minimum_required(VERSION 3.10)
project(codec-atrac C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

file(GLOB_RECURSE SRC_FILES src/*.c src/*.cpp)
add_library(codec-atrac STATIC ${SRC_FILES})
target_include_directories(codec-atrac PUBLIC src)
target_compile_options(codec-atrac PRIVATE -w)
if(NOT WIN32)
  target_link_libraries(codec-atrac PUBLIC m)
endif()

option(ATRAC_BUILD_TESTS "Build the automated tests" ON)
if(ATRAC_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
