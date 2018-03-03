cmake_minimum_required (VERSION 3.7)
project (igel)

include_directories(${PROJECT_SOURCE_DIR})

include_directories("${PROJECT_BINARY_DIR}")

if (WIN32)
    add_executable(igel bitboards.cpp eval.cpp eval_params.cpp main.cpp moves.cpp notation.cpp position.cpp search.cpp utils.cpp win32.cpp)
endif (WIN32)

if (UNIX)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")
    add_executable(igel bitboards.cpp eval.cpp eval_params.cpp main.cpp moves.cpp notation.cpp position.cpp search.cpp utils.cpp unix.cpp)
endif (UNIX)
