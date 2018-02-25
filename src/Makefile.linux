CPP = g++
CPPFLAGS = -Wall -O3

BIN = GreKo

OBJ = bitboards.o   \
      eval.o        \
      eval_params.o \
      learn.o       \
      main.o        \
      moves.o       \
      notation.o    \
      position.o    \
      search.o      \
      utils.o       \
      unix.o

.PHONY: all clean

all: $(BIN)

clean:
	rm -rf $(BIN) $(OBJ)

$(BIN): $(OBJ)
	$(CPP) $(CPPFLAGS) $(OBJ) -o $(BIN)
	strip $(BIN)

.o: .cpp
	$(CPP) $(CPPFLAGS) -c $< -o $@

