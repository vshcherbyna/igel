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

#
#   Taken from https://chessprogramming.wikispaces.com/Null%20Move%20Test-Positions
#   Test-positions where Null Move may fail due to zugzwang
#

engine_def_move_time = 10000

def evaluate(mvt, fen):
    # setup engine
    engine = chess.uci.popen_engine("../igel")
    
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
    sbest = board.san(bestmove)
    print("best move: %s" % sbest)
    if elapsed > (engine_move_time + 100):
        engine.quit()
        raise Exception("exceeded a go time", elapsed)
    print (handler.info["score"][1])
    print("search time: %d ms" % elapsed)
    engine.quit()
    return board.san(bestmove)

def fen_assert(engine_move,  expected_move):
    if engine_move != expected_move:
            raise Exception(engine_move, expected_move)
    
def non_passing_tests():
    fen_assert("g5",  evaluate(None, "7k/5K2/5P1p/3p4/6P1/3p4/8/8 w - - 0 1"))
    fen_assert("Qxh4",  evaluate(None, "8/6B1/p5p1/Pp4kp/1P5r/5P1Q/4q1PK/8 w - - 0 32"))
    fen_assert("Nxd5",  evaluate(None, "8/8/1p1r1k2/p1pPN1p1/P3KnP1/1P6/8/3R4 b - - 0 1"))
    
def main():
    fen_assert("Rf1",  evaluate(None, "8/8/p1p5/1p5p/1P5p/8/PPP2K1p/4R1rk w - - 0 1"))
    fen_assert("Kh6",  evaluate(None, "1q1k4/2Rr4/8/2Q3K1/8/8/8/8 w - - 0 1")) 

if __name__ == "__main__":
    main()
