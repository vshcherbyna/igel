#
# This makefile is used to compile Igel for OpenBench and TCEC
#

CC  = clang
EXE = igel
SRC = *.cpp fathom/tbprobe.cpp

GCCDEFINES = $(shell echo | $(CC) -m64 -march=native -E -dM -)

EVALFILE = weights/c049c117
NNFLAGS  = -DEVALFILE=\"$(EVALFILE)\"

LIBS   = -std=c++20 -mpopcnt -pthread -lstdc++ -lm
WARN   = -Wall
OPTIM  = -O3 -march=native -flto=auto
DEFS   = -DNDEBUG -D_BTYPE=0 -DSYZYGY_SUPPORT=TRUE

ifneq ($(findstring __AVX2__, $(GCCDEFINES)),)
    LIBS += -mavx2
    DEFS += -DUSE_AVX2=1
endif

#
#ifneq ($(findstring __AVX512VNNI__, $(GCCDEFINES)),)
#    LIBS += -mavx512vnni
#    DEFS += -DUSE_AVX512=1 -DUSE_VNNI=1
#endif
#

CFLAGS = $(WARN) $(LIBS) $(OPTIM) $(NNFLAGS)

pgo: download-network
ifeq ($(findstring g++, $(CC)), g++)
	$(CC) $(CFLAGS) $(SRC) $(DEFS) -fprofile-generate=pgo -o $(EXE)
	./$(EXE) bench 14 > pgo.out 2>&1
	$(CC) $(CFLAGS) $(SRC) $(DEFS) -fprofile-use=pgo -o $(EXE)
	@rm -rf pgo pgo.out
else ifeq ($(findstring clang, $(CC)), clang)
	$(CC) $(CFLAGS) $(SRC) $(DEFS) -fprofile-instr-generate -o $(EXE)
	./$(EXE) bench 14 > pgo.out 2>&1
	llvm-profdata merge -output=igel.profdata *.profraw
	$(CC) $(CFLAGS) $(SRC) $(DEFS) -fprofile-instr-use=igel.profdata -o $(EXE)
	@rm -rf pgo pgo.out igel.profdata *.profraw
endif

basic:
	$(CC) $(CFLAGS) $(SRC) $(DEFS) -o $(EXE)

download-network:
	mkdir weights; \
	echo "Downloading latest Igel network with wget"; wget -P weights https://github.com/vshcherbyna/igel/releases/download/3.5.0/c049c117;
