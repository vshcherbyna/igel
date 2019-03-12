cmake_minimum_required (VERSION 3.7)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project (igel)

include_directories(${PROJECT_SOURCE_DIR})

FILE(GLOB SRCFILES ${PROJECT_SOURCE_DIR}/src/*.cpp)
FILE(GLOB HDRFILES ${PROJECT_SOURCE_DIR}/src/*.h)

FILE(GLOB FATHOM_SRCFILES ${PROJECT_SOURCE_DIR}/src/fathom/*.c)
FILE(GLOB FATHOM_HDRFILES ${PROJECT_SOURCE_DIR}/src/fathom/*.h)

if (MSVC)
    SET(CMAKE_CXX_FLAGS_DEBUG "/MTd /Zi /Ob0 /Od /RTC1")
    SET(CMAKE_CXX_FLAGS_RELEASE "/MT /O2 /Ob2 /Oi /Ot /Oy /GL /GS- /DNDEBUG")
    SET(CMAKE_CXX_STACK_SIZE "15728640")
endif (MSVC)

if (UNIX)
    add_definitions(-DNDEBUG)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -O3 -march=barcelona -pthread")
endif (UNIX)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set (CMAKE_C_FLAGS "-std=gnu99 ${CMAKE_C_FLAGS}")
endif ()

add_executable(igel ${SRCFILES} ${HDRFILES} ${FATHOM_SRCFILES} ${FATHOM_HDRFILES})

add_subdirectory(external/googletest)
enable_testing()
include_directories(${googletest_SOURCE_DIR}/include ${googletest_SOURCE_DIR})
FILE(GLOB UTSRCFILES ${PROJECT_SOURCE_DIR}/src/unit/*.cpp)
add_executable(unit ${UTSRCFILES} ${SRCFILES} ${HDRFILES} ${FATHOM_SRCFILES} ${FATHOM_HDRFILES})
target_link_libraries(unit gtest gtest_main)
target_compile_definitions(unit PRIVATE UNIT_TEST)
