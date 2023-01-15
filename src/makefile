#
# This makefile is solely designed to compile Igel for OpenBench
# Official build method for Igel still requires cmake
#

CC  = g++
EXE = igel
SRC = *.cpp fathom/tbprobe.cpp

GCCDEFINES = $(shell echo | $(CC) -m64 -march=native -E -dM -)

EVALFILE = weights/ign-2-51ba2968
NNFLAGS  = -DEVALFILE=\"$(EVALFILE)\"

LIBS   = -std=c++17 -mpopcnt -pthread
WARN   = -Wall
OPTIM  = -O3 -march=native -flto
DEFS   = -DNDEBUG -D_BTYPE=0 -DSYZYGY_SUPPORT=TRUE

ifneq ($(findstring __AVX2__, $(GCCDEFINES)),)
    LIBS += -mavx2
    DEFS += -DUSE_AVX2=1
endif

ifneq ($(findstring __AVX512VNNI__, $(GCCDEFINES)),)
    LIBS += -mavx512vnni
    DEFS += -DUSE_AVX512=1 -DUSE_VNNI=1
endif

CFLAGS = $(WARN) $(LIBS) $(OPTIM) $(NNFLAGS)

basic:
	$(CC) $(CFLAGS) $(SRC) $(DEFS) -o $(EXE)
