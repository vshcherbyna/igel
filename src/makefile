#
# This makefile is used to compile Igel for OpenBench and TCEC
#

CC  = g++
EXE = igel
SRC = *.cpp fathom/tbprobe.cpp

GCCDEFINES = $(shell echo | $(CC) -m64 -march=native -E -dM -)

EVALFILE = weights/c049c117
NNFLAGS  = -DEVALFILE=\"$(EVALFILE)\"

LIBS   = -std=c++17 -mpopcnt -pthread -lstdc++ -lm
WARN   = -Wall
OPTIM  = -O3 -march=native -flto=auto -funroll-loops
DEFS   = -DNDEBUG -D_BTYPE=0 -DSYZYGY_SUPPORT=TRUE

ifneq ($(findstring __AVX2__, $(GCCDEFINES)),)
    LIBS += -mavx2
    DEFS += -DUSE_AVX2=1
endif

ifneq ($(findstring __AVX512BW__, $(GCCDEFINES)),)
    LIBS += -mavx512f -mavx512bw
    DEFS += -DUSE_AVX512=1
endif

ifneq ($(findstring __AVX512VNNI__, $(GCCDEFINES)),)
    LIBS += -mavx512vnni
    DEFS += -DUSE_VNNI=1
endif

CFLAGS = $(WARN) $(LIBS) $(OPTIM) $(NNFLAGS)

#
# Clang full-LTO + PGO build settings (used by the pgoclang target below).
# Clang rejects gcc's -flto=auto and links LTO through lld. Override
# CLANGCC / CLANGLD / PROFDATA if your toolchain uses unversioned names.
#
CLANGCC    = clang++-19
CLANGLD    = lld-19
PROFDATA   = llvm-profdata-19
CLANGOPTIM = -O3 -march=native -flto -fuse-ld=$(CLANGLD) -funroll-loops
CLANGFLAGS = $(WARN) $(LIBS) $(CLANGOPTIM) $(NNFLAGS)

basic:
	$(CC) $(CFLAGS) $(SRC) $(DEFS) -o $(EXE)

pgo: download-network
ifeq ($(findstring g++, $(CC)), g++)
	$(CC) $(CFLAGS) $(SRC) $(DEFS) -fprofile-generate=pgo -o $(EXE)
	./$(EXE) bench 15 > pgo.out 2>&1
	$(CC) $(CFLAGS) $(SRC) $(DEFS) -fprofile-use=pgo -o $(EXE)
	@rm -rf pgo pgo.out
else ifeq ($(findstring clang, $(CC)), clang)
	$(CC) $(CFLAGS) $(SRC) $(DEFS) -fprofile-instr-generate -o $(EXE)
	./$(EXE) bench 15 > pgo.out 2>&1
	llvm-profdata merge -output=igel.profdata *.profraw
	$(CC) $(CFLAGS) $(SRC) $(DEFS) -fprofile-instr-use=igel.profdata -o $(EXE)
	@rm -rf pgo pgo.out igel.profdata *.profraw
endif

#
# Clang full-LTO + Profile-Guided Optimization build (sibling of 'pgo').
# A single 'make pgoclang' builds an instrumented binary, trains it on
# bench, merges the profile with llvm-profdata, then rebuilds with it.
#
pgoclang: download-network
	$(CLANGCC) $(CLANGFLAGS) $(SRC) $(DEFS) -fprofile-generate -o $(EXE)
	LLVM_PROFILE_FILE=igel.profraw ./$(EXE) bench 15 > pgo.out 2>&1
	$(PROFDATA) merge -output=igel.profdata igel.profraw
	$(CLANGCC) $(CLANGFLAGS) $(SRC) $(DEFS) -fprofile-use=igel.profdata -Wno-profile-instr-unprofiled -Wno-profile-instr-out-of-date -o $(EXE)
	@rm -rf igel.profraw igel.profdata pgo.out default*.profraw

#
# Same as the pgoclang target but builds the pure hand-crafted evaluator
# (-DPURE_HCE). The HCE engine embeds no NNUE network, so there is no
# download-network step.
#
pgoclanghce:
	$(CLANGCC) $(CLANGFLAGS) $(SRC) $(DEFS) -DPURE_HCE -fprofile-generate -o $(EXE)
	LLVM_PROFILE_FILE=igel.profraw ./$(EXE) bench 15 > pgo.out 2>&1
	$(PROFDATA) merge -output=igel.profdata igel.profraw
	$(CLANGCC) $(CLANGFLAGS) $(SRC) $(DEFS) -DPURE_HCE -fprofile-use=igel.profdata -Wno-profile-instr-unprofiled -Wno-profile-instr-out-of-date -o $(EXE)
	@rm -rf igel.profraw igel.profdata pgo.out default*.profraw

download-network:
	mkdir weights; \
	echo "Downloading latest Igel network with wget"; wget -P weights https://github.com/vshcherbyna/igel/releases/download/3.5.0/c049c117;
