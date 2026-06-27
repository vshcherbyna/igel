cmake_minimum_required (VERSION 3.7)
SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_EXTENSIONS OFF)
PROJECT(igel)

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})

FILE(GLOB SRCFILES ${PROJECT_SOURCE_DIR}/src/*.cpp)
FILE(GLOB HDRFILES ${PROJECT_SOURCE_DIR}/src/*.h)

FILE(GLOB FATHOM_SRCFILES ${PROJECT_SOURCE_DIR}/src/fathom/*.cpp)
FILE(GLOB FATHOM_HDRFILES ${PROJECT_SOURCE_DIR}/src/fathom/*.h)
FILE(GLOB INCBIN_HDRFILES ${PROJECT_SOURCE_DIR}/src/incbin/*.h)

IF (DEFINED EVALFILE)
    ADD_DEFINITIONS(-DEVALFILE=\"${EVALFILE}\")
ENDIF()

IF (DEFINED USE_PEXT)
    ADD_DEFINITIONS(-DUSE_PEXT=${USE_PEXT})
ENDIF()

IF (DEFINED USE_AVX2)
    ADD_DEFINITIONS(-DUSE_AVX2=${USE_AVX2})
ENDIF()
IF (DEFINED USE_AVX512)
    ADD_DEFINITIONS(-DUSE_AVX512=${USE_AVX512})
ENDIF()
IF (DEFINED USE_VNNI)
    ADD_DEFINITIONS(-DUSE_VNNI=${USE_VNNI})
ENDIF()

IF (DEFINED _BTYPE)
    ADD_DEFINITIONS(-D_BTYPE=${_BTYPE})
    IF (UNIX)
        IF (_BTYPE STREQUAL "1")
            SET(CMAKE_CXX_FLAGS "-mbmi2 ${CMAKE_CXX_FLAGS}")
        ENDIF()
    ENDIF()
ENDIF()

IF (DEFINED SYZYGY_SUPPORT)
    ADD_DEFINITIONS(-DSYZYGY_SUPPORT=${SYZYGY_SUPPORT})
ENDIF()

IF (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    SET(CMAKE_C_FLAGS "-std=gnu99 ${CMAKE_C_FLAGS}")
ENDIF ()

IF (MSVC)
    SET(CMAKE_CXX_FLAGS_DEBUG "/MTd /Zi /Ob0 /Od /RTC1")
    SET(CMAKE_CXX_FLAGS_RELEASE "/MT /O2 /Ob2 /Oi /Ot /Oy /GL /GS- /DNDEBUG")
    IF (DEFINED USE_AVX512)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX512")
    ENDIF()
ENDIF (MSVC)

IF (DEFINED _MAKE_UNIT_TEST)
    IF (UNIX)
        ADD_DEFINITIONS(-DNDEBUG)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wno-error=maybe-uninitialized -O3 -march=native -pthread")
    ENDIF (UNIX)
    ADD_SUBDIRECTORY(external/googletest)
    ENABLE_TESTING()
    INCLUDE_DIRECTORIES(${googletest_SOURCE_DIR}/include ${googletest_SOURCE_DIR})
    FILE(GLOB UTSRCFILES ${PROJECT_SOURCE_DIR}/src/unit/*.cpp)
    ADD_EXECUTABLE(unit ${UTSRCFILES} ${SRCFILES} ${HDRFILES} ${FATHOM_SRCFILES} ${FATHOM_HDRFILES})
    TARGET_LINK_LIBRARIES(unit gtest gtest_main)
    TARGET_COMPILE_DEFINITIONS(unit PRIVATE ${_MAKE_UNIT_TEST})
ELSE()
    IF (UNIX)
        ADD_DEFINITIONS(-DNDEBUG)
        IF (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            # Clang rejects gcc's -flto=auto and needs an LTO-capable linker (lld).
            SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O3 -march=native -flto -fuse-ld=lld-19 -funroll-loops -pthread -mavx2")
        ELSE()
            SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -O3 -march=native -flto=auto -funroll-loops -pthread -mavx2")
        ENDIF()
        IF (DEFINED USE_AVX512)
            SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx512f -mavx512bw")
            IF (DEFINED USE_VNNI)
                SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx512vnni")
            ENDIF()
        ENDIF()
    ENDIF (UNIX)

    SET(IGEL_SOURCES ${SRCFILES} ${HDRFILES} ${FATHOM_SRCFILES} ${FATHOM_HDRFILES} ${INCBIN_HDRFILES})

    IF (DEFINED PGO)
        #
        #  Profile-Guided Optimization (Clang IRPGO + full LTO).
        #  A single 'make' does the whole cycle: build an instrumented binary,
        #  train it on 'bench', merge the profile, then rebuild 'igel' with it.
        #
        IF (NOT UNIX OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            MESSAGE(FATAL_ERROR "PGO build supports Clang on UNIX. Re-run cmake with -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++")
        ENDIF()

        FIND_PROGRAM(LLVM_PROFDATA NAMES llvm-profdata-19 llvm-profdata)
        IF (NOT LLVM_PROFDATA)
            MESSAGE(FATAL_ERROR "llvm-profdata not found - install the matching LLVM tools (e.g. llvm-19) for a PGO build")
        ENDIF()

        SET(PGO_DIR  ${CMAKE_BINARY_DIR}/pgo)
        SET(PGO_RAW  ${PGO_DIR}/igel.profraw)
        SET(PGO_DATA ${PGO_DIR}/igel.profdata)
        FILE(MAKE_DIRECTORY ${PGO_DIR})

        # Stage 1: instrumented binary (kept out of 'all'; built on demand below).
        ADD_EXECUTABLE(igel_instrumented EXCLUDE_FROM_ALL ${IGEL_SOURCES})
        TARGET_COMPILE_OPTIONS(igel_instrumented PRIVATE -fprofile-generate)
        SET_TARGET_PROPERTIES(igel_instrumented PROPERTIES LINK_FLAGS "-fprofile-generate")

        # Stage 2: train on 'bench' and merge the raw counters into a .profdata.
        ADD_CUSTOM_COMMAND(
            OUTPUT ${PGO_DATA}
            COMMAND ${CMAKE_COMMAND} -E rm -f ${PGO_RAW} ${PGO_DATA}
            COMMAND ${CMAKE_COMMAND} -E env LLVM_PROFILE_FILE=${PGO_RAW} $<TARGET_FILE:igel_instrumented> bench
            COMMAND ${LLVM_PROFDATA} merge -output=${PGO_DATA} ${PGO_RAW}
            DEPENDS igel_instrumented
            COMMENT "PGO: training on bench and merging profile"
            VERBATIM)
        ADD_CUSTOM_TARGET(igel_pgo_profile DEPENDS ${PGO_DATA})

        # Stage 3: the real 'igel', optimized with the collected profile.
        ADD_EXECUTABLE(igel ${IGEL_SOURCES})
        TARGET_COMPILE_OPTIONS(igel PRIVATE -fprofile-use=${PGO_DATA} -Wno-profile-instr-unprofiled -Wno-profile-instr-out-of-date)
        ADD_DEPENDENCIES(igel igel_pgo_profile)
    ELSE()
        ADD_EXECUTABLE(igel ${IGEL_SOURCES})
    ENDIF()
ENDIF()
