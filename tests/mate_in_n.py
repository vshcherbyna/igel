#
#  Igel - a UCI chess playing engine derived from GreKo 2018.01
#
#  Copyright (C) 2018-2020 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

#!/usr/bin/python3
 
import chess
import chess.engine
import timeit

engine_def_move_time = 1000

def evaluate(thr, mvt, mate,  fen):
    # setup engine
    engine = chess.engine.SimpleEngine.popen_uci("../igel")
    engine.configure({"Threads": thr})

    # setup board
    board = chess.Board(fen)
    engine_move_time = engine_def_move_time

    if mvt != None:
        engine_move_time = mvt

    # run engine
    start_time = timeit.default_timer()
    info = engine.analyse(board, chess.engine.Limit(time=engine_move_time))
    elapsed = timeit.default_timer() - start_time
    elapsed = elapsed * 1000
    print(fen)
    print(board)
    print(info)
    print("search time: %d ms" % elapsed)
    if elapsed > ((1000 * engine_move_time) + 200):
        engine.quit()
        raise Exception("exceeded a go time", elapsed)
    expected_mate = "#"
    if mate > 0:
        expected_mate += "+"
    expected_mate += str(mate)
    obtained_mate = str(info["score"])

    if expected_mate != obtained_mate:
        raise Exception(expected_mate, obtained_mate)
    engine.quit()

    bestmove = info["pv"]
    return board.san(bestmove[0])

def fen_assert(engine_move,  expected_move):
    if engine_move != expected_move:
            raise Exception(engine_move, expected_move)
    
def fnc_mate_in_n(thr):
    fen_assert("Qg8#",  evaluate(thr, 1, 1, "rnbqk3/ppppp3/8/6Q1/3P4/4P3/PPP1BPPP/RNB1K1NR w - - 0 1"))
    fen_assert("Qxh2+",  evaluate(thr, 1, 2, "4r1k1/pQ3pp1/7p/4q3/4r3/P7/1P2nPPP/2BR1R1K b - - 0 1"))
    fen_assert("Rg6+",  evaluate(thr, 2, 3, "4r1k1/3n1ppp/4r3/3n3q/Q2P4/5P2/PP2BP1P/R1B1R1K1 b - - 0 1"))
    fen_assert("Rfe7+",  evaluate(thr, 3, 4, "4k2r/1R3R2/p3p1pp/4b3/1BnNr3/8/P1P5/5K2 w - - 1 0"))
    fen_assert("Rxh6+",  evaluate(thr, 4, 5, "6r1/p3p1rk/1p1pPp1p/q3n2R/4P3/3BR2P/PPP2QP1/7K w - - 0 1"))
    fen_assert("Bxf5",  evaluate(thr, 7, 6, "1r3r2/3p4/2bNpB2/p3Pp1k/6R1/1p1B2P1/PP3P1P/6K1 w - - 0 1"))
    fen_assert("Qe6+",  evaluate(thr, 15, 7, "2k1rr2/ppp3p1/7n/2N1p3/2Q5/2PP3p/1P1q3P/R4R1K w - - 0 1"))
    fen_assert("Nc7+",  evaluate(thr, 15, 8, "k7/pp6/1n2N2p/3p1Ppq/8/PP2P1BP/8/K1Q5 w - - 0 1"))
