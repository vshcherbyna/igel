#
# This make file is solely designed to compile Igel for OpenBench
# Official build method for Igel still requires cmake
#

EXE = igel
SRC = *.cpp

# Defines (0 stands for POPCNT, 1 stands for BMI2)
# BMI2 is disabled for OpenBench testing as most machines used are AMD

POPC = -D_BTYPE=0

# Flags
LIBS   = -pthread
WARN   = -Wall
OPTIM  = -O3 -march=native -flto

CFLAGS = $(WARN) $(LIBS) $(OPTIM) 

basic:
	g++ $(CFLAGS) $(SRC) $(POPC) -DOB_ADJUDICATION_OFF=1 -DNDEBUG -o $(EXE)
