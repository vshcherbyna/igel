cmake_minimum_required (VERSION 3.7)
project (igel)

set (Tutorial_VERSION_MAJOR 1)
set (Tutorial_VERSION_MINOR 0)

include_directories("${PROJECT_BINARY_DIR}")

if (WIN32)
    add_executable(igel bitboards.cpp eval.cpp eval_params.cpp main.cpp moves.cpp notation.cpp position.cpp search.cpp utils.cpp win32.cpp learn.cpp)
endif (WIN32)

if (UNIX)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")
    add_executable(igel bitboards.cpp eval.cpp eval_params.cpp main.cpp moves.cpp notation.cpp position.cpp search.cpp utils.cpp unix.cpp learn.cpp)
endif (UNIX)
