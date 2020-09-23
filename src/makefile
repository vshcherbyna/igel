#
# This makefile is solely designed to compile Igel for OpenBench
# Official build method for Igel still requires cmake
#

CC  = g++
EXE = igel
SRC = *.cpp nnue/*.cpp nnue/features/*.cpp

GCCDEFINES = $(shell echo | $(CC) -m64 -march=native -E -dM -)

LIBS   = -std=c++17 -mpopcnt -pthread
WARN   = -Wall
OPTIM  = -O3 -march=native -flto
DEFS   = -DNDEBUG -DEVAL_NNUE=1 -D_BTYPE=0

ifneq ($(findstring __SSE4_1__, $(GCCDEFINES)),)
    LIBS += -msse -msse2 -msse3 -msse4.1
    DEFS += -DUSE_SSE2=1 -DUSE_SSSE3=1 -DUSE_SSE41=1
endif

ifneq ($(findstring __AVX2__, $(GCCDEFINES)),)
    LIBS += -mavx2
    DEFS += -DUSE_AVX2=1
endif

ifneq ($(findstring __AVX512__, $(GCCDEFINES)),)
    LIBS += -mavx512bw
    DEFS += -DUSE_AVX512=1
endif

CFLAGS = $(WARN) $(LIBS) $(OPTIM)

basic:
	$(CC) $(CFLAGS) $(SRC) $(DEFS) -o $(EXE)
