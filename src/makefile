#
# This makefile is used to compile Igel for OpenBench and TCEC
#

CC  = g++
EXE = igel
SRC = *.cpp fathom/tbprobe.cpp

#
# NNUE network: embedded via INCBIN at compile time, and fetched by the
# $(EVALFILE) rule below if it is missing. Change these to switch networks.
#

EVALFILE    = weights/3.6.14.nnue
NETWORK_URL = https://github.com/vshcherbyna/igel/releases/download/0.8/3.6.14.nnue

GCCDEFINES = $(shell echo | $(CC) -m64 -march=native -E -dM -)

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

pgo: $(EVALFILE)
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

pgoclang: $(EVALFILE)
	$(CLANGCC) $(CLANGFLAGS) $(SRC) $(DEFS) -fprofile-generate -o $(EXE)
	LLVM_PROFILE_FILE=igel.profraw ./$(EXE) bench 15 > pgo.out 2>&1
	$(PROFDATA) merge -output=igel.profdata igel.profraw
	$(CLANGCC) $(CLANGFLAGS) $(SRC) $(DEFS) -fprofile-use=igel.profdata -Wno-profile-instr-unprofiled -Wno-profile-instr-out-of-date -o $(EXE)
	@rm -rf igel.profraw igel.profdata pgo.out default*.profraw

#
# Same as the pgoclang target but builds the pure hand-crafted evaluator
# (-DPURE_HCE). The HCE engine embeds no NNUE network, so it needs no
# $(EVALFILE) prerequisite.
#

pgoclanghce:
	$(CLANGCC) $(CLANGFLAGS) $(SRC) $(DEFS) -DPURE_HCE -fprofile-generate -o $(EXE)
	LLVM_PROFILE_FILE=igel.profraw ./$(EXE) bench 15 > pgo.out 2>&1
	$(PROFDATA) merge -output=igel.profdata igel.profraw
	$(CLANGCC) $(CLANGFLAGS) $(SRC) $(DEFS) -DPURE_HCE -fprofile-use=igel.profdata -Wno-profile-instr-unprofiled -Wno-profile-instr-out-of-date -o $(EXE)
	@rm -rf igel.profraw igel.profdata pgo.out default*.profraw

#
# Fetch the NNUE network once, only if it is missing (fresh checkout or CI). A
# failed or interrupted download is removed by .DELETE_ON_ERROR and retried on
# the next run, so a truncated network is never embedded.
#

.DELETE_ON_ERROR:

$(EVALFILE):
	mkdir -p $(dir $@)
	wget -O $@ $(NETWORK_URL)
