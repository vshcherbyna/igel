cmake_minimum_required (VERSION 3.7)

#
#  A PGO build (-DPGO=1) is Clang full-LTO + Profile-Guided Optimization, the
#  same recipe as 'make pgoclang' in src/makefile. Select the LLVM 19 toolchain
#  automatically so -DPGO=1 needs no extra flags; an explicit
#  -DCMAKE_CXX_COMPILER=... still wins. Configure a PGO build in a clean tree.
#
IF (PGO AND NOT DEFINED CMAKE_CXX_COMPILER)
    SET(CMAKE_C_COMPILER   clang-19)
    SET(CMAKE_CXX_COMPILER clang++-19)
ENDIF()

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
IF (DEFINED USE_AVXVNNI)
    ADD_DEFINITIONS(-DUSE_AVXVNNI=${USE_AVXVNNI})
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
    ELSEIF (DEFINED USE_AVX2)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2")
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
        IF (DEFINED USE_AVXVNNI)
            SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavxvnni")
        ENDIF()
    ENDIF (UNIX)

    SET(IGEL_SOURCES ${SRCFILES} ${HDRFILES} ${FATHOM_SRCFILES} ${FATHOM_HDRFILES} ${INCBIN_HDRFILES})

    IF (PGO)
        #
        #  Profile-Guided Optimization: the same four steps as 'make pgoclang'
        #  in src/makefile, run by a single 'make'. The clang full-LTO flags and
        #  the LLVM 19 toolchain are set above, so only the profile flags differ
        #  between the instrumented and the final build.
        #
        IF (NOT UNIX OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            MESSAGE(FATAL_ERROR "PGO is a Clang-on-UNIX build; install the LLVM 19 toolchain (clang-19, lld-19, llvm-profdata-19) and configure in a clean tree.")
        ENDIF()

        FIND_PROGRAM(LLVM_PROFDATA NAMES llvm-profdata-19 llvm-profdata)
        IF (NOT LLVM_PROFDATA)
            MESSAGE(FATAL_ERROR "llvm-profdata not found - install the LLVM 19 tools for a PGO build")
        ENDIF()

        SET(PGO_RAW  ${CMAKE_BINARY_DIR}/igel.profraw)
        SET(PGO_DATA ${CMAKE_BINARY_DIR}/igel.profdata)

        # 1. Build an instrumented igel (-fprofile-generate); kept out of 'all'.
        ADD_EXECUTABLE(igel_instrumented EXCLUDE_FROM_ALL ${IGEL_SOURCES})
        TARGET_COMPILE_OPTIONS(igel_instrumented PRIVATE -fprofile-generate)
        SET_TARGET_PROPERTIES(igel_instrumented PROPERTIES LINK_FLAGS "-fprofile-generate")

        # 2. Train it on 'bench' and 3. merge the counters into igel.profdata.
        ADD_CUSTOM_COMMAND(
            OUTPUT ${PGO_DATA}
            COMMAND ${CMAKE_COMMAND} -E env LLVM_PROFILE_FILE=${PGO_RAW} $<TARGET_FILE:igel_instrumented> bench 15
            COMMAND ${LLVM_PROFDATA} merge -output=${PGO_DATA} ${PGO_RAW}
            DEPENDS igel_instrumented
            COMMENT "PGO: training on bench and merging profile"
            VERBATIM)
        ADD_CUSTOM_TARGET(igel_profile DEPENDS ${PGO_DATA})

        # 4. Rebuild the real igel with the profile (-fprofile-use), at compile
        #    and link so full-LTO code generation uses it too.
        ADD_EXECUTABLE(igel ${IGEL_SOURCES})
        ADD_DEPENDENCIES(igel igel_profile)
        TARGET_COMPILE_OPTIONS(igel PRIVATE -fprofile-use=${PGO_DATA} -Wno-profile-instr-unprofiled -Wno-profile-instr-out-of-date)
        SET_TARGET_PROPERTIES(igel PROPERTIES LINK_FLAGS "-fprofile-use=${PGO_DATA}")
    ELSE()
        ADD_EXECUTABLE(igel ${IGEL_SOURCES})
    ENDIF()
ENDIF()
