#
# This makefile is used to compile Igel for OpenBench and TCEC
#

#
# Toolchain. CC builds the "basic" target; the CLANG* names are the LLVM
# programs the pgo targets drive. Override any of them on the command line
# when your toolchain spells them differently - an MSYS2 clang64 install,
# for instance, ships them unversioned:
#
#     make pgo CLANGCC=clang++ CLANGLD=lld PROFDATA=llvm-profdata
#

CC       = g++
CLANGCC  = clang++-19
CLANGLD  = lld-19
PROFDATA = llvm-profdata-19

EXE      = igel
SRC      = *.cpp fathom/tbprobe.cpp

#
# NNUE network: embedded via INCBIN at compile time, and fetched by the
# $(EVALFILE) rule below if it is missing. Change these to switch networks.
#

EVALFILE    = weights/020217C0.network
NETWORK_URL = https://github.com/vshcherbyna/igel/releases/download/0.8/020217C0.network

GCCDEFINES = $(shell echo | $(CC) -m64 -march=native -E -dM -)

NNFLAGS  = -DEVALFILE=\"$(EVALFILE)\"

LIBS   = -std=c++17 -mpopcnt -pthread -lstdc++ -lm
WARN   = -Wall
OPTIM  = -O3 -march=native -flto=auto -funroll-loops
BTYPE  = 0
DEFS   = -DNDEBUG -D_BTYPE=$(BTYPE) -DSYZYGY_SUPPORT=TRUE

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

ifneq ($(findstring __AVXVNNI__, $(GCCDEFINES)),)
    LIBS += -mavxvnni
    DEFS += -DUSE_AVXVNNI=1
endif

ifneq ($(findstring __BMI2__, $(GCCDEFINES)),)
ifeq ($(findstring __znver1, $(GCCDEFINES)),)
ifeq ($(findstring __znver2, $(GCCDEFINES)),)
    BTYPE = 1
    LIBS += -mbmi2
endif
endif
endif

ifneq ($(findstring __MINGW32__, $(GCCDEFINES)),)
    LIBS += -static
endif

CFLAGS = $(WARN) $(LIBS) $(OPTIM) $(NNFLAGS)

#
# Clang full-LTO + PGO build settings (used by the pgo targets below).
# Clang rejects gcc's -flto=auto and links LTO through lld. The program
# names themselves are declared at the top of this file.
#

CLANGOPTIM = -O3 -march=native -flto -fuse-ld=$(CLANGLD) -funroll-loops
CLANGFLAGS = $(WARN) $(LIBS) $(CLANGOPTIM) $(NNFLAGS)

basic:
	$(CC) $(CFLAGS) $(SRC) $(DEFS) -o $(EXE)

#
# Clang full-LTO + Profile-Guided Optimization build. A single 'make pgo'
# builds an instrumented binary, trains it on bench, merges the profile with
# llvm-profdata, then rebuilds with it.
#

pgo: $(EVALFILE)
	$(CLANGCC) $(CLANGFLAGS) $(SRC) $(DEFS) -fprofile-generate -o $(EXE)
	LLVM_PROFILE_FILE=igel.profraw ./$(EXE) bench 15 > pgo.out 2>&1
	$(PROFDATA) merge -output=igel.profdata igel.profraw
	$(CLANGCC) $(CLANGFLAGS) $(SRC) $(DEFS) -fprofile-use=igel.profdata -Wno-profile-instr-unprofiled -Wno-profile-instr-out-of-date -o $(EXE)
	@rm -rf igel.profraw igel.profdata pgo.out default*.profraw

#
# Same as the pgo target but builds the pure hand-crafted evaluator
# (-DPURE_HCE). The HCE engine embeds no NNUE network, so it needs no
# $(EVALFILE) prerequisite.
#

pgo_hce:
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
