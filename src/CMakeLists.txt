cmake_minimum_required (VERSION 3.7)
project (igel)

include_directories(${PROJECT_SOURCE_DIR})

FILE(GLOB SRCFILES ${PROJECT_SOURCE_DIR}/*.cpp)
FILE(GLOB HDRFILES ${PROJECT_SOURCE_DIR}/*.h)

if (UNIX)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")
endif (UNIX)

add_executable(igel ${SRCFILES} ${HDRFILES})
