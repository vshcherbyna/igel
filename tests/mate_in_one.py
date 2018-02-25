#!/usr/bin/python
 
import chess
import chess.uci
import datetime

engine_move_time = 10
engine_memory = 1

def evaluate(fen):
    # setup engine
    engine = chess.uci.popen_engine("../src/igel")
    engine.setoption({"Hash": engine_memory}) 
    
    # setup new game
    engine.uci()
    engine.ucinewgame()
    
    # setup board
    board = chess.Board(fen)
    handler = chess.uci.InfoHandler()
    print(board)

    # run engine
    engine.info_handlers.append(handler)
    engine.position(board)
    a = datetime.datetime.now()
    bestmove, pondermove = engine.go(movetime=engine_move_time)
    b = datetime.datetime.now()
    d = (b - a).microseconds
    if d > 10000:
        raise Exception("exceeded a go time", d)
    print("search time: %d" % d)
    engine.quit()

    return board.san(bestmove)

def fen_assert(engine_move,  expected_move):
    if engine_move != expected_move:
            raise Exception(engine_move, expected_move)
    
def main():
    fen_assert("Qg8#",  evaluate("rnbqk3/ppppp3/8/6Q1/3P4/4P3/PPP1BPPP/RNB1K1NR w - - 0 1"))

if __name__ == "__main__":
    main()
