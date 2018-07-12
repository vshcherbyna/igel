#
#  Igel - a UCI chess playing engine derived from GreKo 2018.01
#
#  Copyright (C) 2018 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
#
#  Igel is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  Igel is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with Igel.  If not, see <http://www.gnu.org/licenses/>.
#

#!/usr/bin/python
 
import chess
import chess.uci
import timeit

engine_def_move_time = 1000

def evaluate(mvt, mate,  fen):
    # setup engine
    engine = chess.uci.popen_engine("../igel")
    engine.setoption({"Threads": 1})
    
    # setup new game
    engine.uci()
    engine.ucinewgame()
    
    # setup board
    board = chess.Board(fen)
    handler = chess.uci.InfoHandler()
    print(fen)
    print(board)

    engine_move_time = engine_def_move_time

    if mvt != None:
        engine_move_time = mvt
        
    # run engine
    engine.info_handlers.append(handler)
    engine.position(board)
    start_time = timeit.default_timer()
    bestmove, pondermove = engine.go(movetime=engine_move_time)
    elapsed = timeit.default_timer() - start_time
    elapsed = elapsed * 1000
    if elapsed > (engine_move_time + 100):
        engine.quit()
        raise Exception("exceeded a go time", elapsed)
    print (handler.info["score"][1])
    print("search time: %d ms" % elapsed)
    print("mate in %d" % handler.info["score"][1].mate)
    if mate != handler.info["score"][1].mate:
        raise Exception(mate, handler.info["score"][1].mate)
    engine.quit()

    return board.san(bestmove)

def fen_assert(engine_move,  expected_move):
    if engine_move != expected_move:
            raise Exception(engine_move, expected_move)
    
def main():
    fen_assert("Qg8#",  evaluate(1000, 1, "rnbqk3/ppppp3/8/6Q1/3P4/4P3/PPP1BPPP/RNB1K1NR w - - 0 1"))
    fen_assert("Qxh2+",  evaluate(1000, 2, "4r1k1/pQ3pp1/7p/4q3/4r3/P7/1P2nPPP/2BR1R1K b - - 0 1"))
    fen_assert("Rg6+",  evaluate(2000, 3, "4r1k1/3n1ppp/4r3/3n3q/Q2P4/5P2/PP2BP1P/R1B1R1K1 b - - 0 1"))
    fen_assert("Rfe7+",  evaluate(3000, 4, "4k2r/1R3R2/p3p1pp/4b3/1BnNr3/8/P1P5/5K2 w - - 1 0"))
    fen_assert("Rxh6+",  evaluate(4000, 5, "6r1/p3p1rk/1p1pPp1p/q3n2R/4P3/3BR2P/PPP2QP1/7K w - - 0 1"))
    fen_assert("Bxf5",  evaluate(5000, 6, "1r3r2/3p4/2bNpB2/p3Pp1k/6R1/1p1B2P1/PP3P1P/6K1 w - - 0 1"))
    fen_assert("Qe6+",  evaluate(7000, 7, "2k1rr2/ppp3p1/7n/2N1p3/2Q5/2PP3p/1P1q3P/R4R1K w - - 0 1"))
    fen_assert("Nc7+",  evaluate(10000, 8, "k7/pp6/1n2N2p/3p1Ppq/8/PP2P1BP/8/K1Q5 w - - 0 1"))
    fen_assert("Rb7",  evaluate(30000, 9, "3Q4/5q1k/4ppp1/2Kp1N1B/RR6/3P1r2/4nP1b/3b4 w - - 0 1"))
    fen_assert("Qh5+",  evaluate(40000, 10, "2qrr1n1/3b1kp1/2pBpn1p/1p2PP2/p2P4/1BP5/P3Q1PP/4RRK1 w - - 0 1"))
    fen_assert("h5",  evaluate(1000, -3, "7k/4Q2p/8/8/8/8/8/4K2R b K - 0 1"))

if __name__ == "__main__":
    main()
