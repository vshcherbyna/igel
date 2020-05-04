#
# This file is solely designed for OpenBench
# Official complilation method for Igel requires cmake
#

EXE = igel
SRC = *.cpp fathom/tbprobe.c

# Defines
POPC = -D_BTYPE=POPCNT

# Flags
LIBS   = -pthread
WARN   = -Wall
OPTIM  = -O3 -march=native -flto

CFLAGS = $(WARN) $(LIBS) $(OPTIM) 

basic:
	g++ $(CFLAGS) $(SRC) $(POPC) -o $(EXE)
