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
#   Dann Corbit test that is designed to help tune chess engines.
#   First published in CCC. In August 2008, Dann found a novelty in one of the positions
#   Taken from https://chessprogramming.wikispaces.com/Silent%20but%20deadly
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
    fen_assert("Bh6",  evaluate(None, "3r1rk1/p1q4p/1pP1ppp1/2n1b1B1/2P5/6P1/P1Q2PBP/1R3RK1 w - - 0 1"))
    fen_assert("Bc4",  evaluate(None, "3r2k1/p1q1npp1/3r1n1p/2p1p3/4P2B/P1P2Q1P/B4PP1/1R2R1K1 w - - 0 1"))
    fen_assert("O-O",  evaluate(None, "3rk2r/1b2bppp/p1qppn2/1p6/4P3/PBN2PQ1/1PP3PP/R1B1R1K1 b k - 0 1"))
    fen_assert("Be7",  evaluate(None, "3rk2r/1bq2pp1/2pb1n1p/p3pP2/P1B1P3/8/1P2QBPP/2RN1R1K b k - 0 1"))
    fen_assert("O-O",  evaluate(None, "r1b1k1nr/1p3ppp/p1np4/4p1q1/2P1P3/N1NB4/PP3PPP/2RQK2R w Kkq - 0 1"))
    fen_assert("O-O",  evaluate(None, "r1b1k2r/p1pp1ppp/1np1q3/4P3/1bP5/1P6/PB1NQPPP/R3KB1R b KQkq - 0 1"))

def main():
    fen_assert("Qc7",  evaluate(None, "1qr3k1/p2nbppp/bp2p3/3p4/3P4/1P2PNP1/P2Q1PBP/1N2R1K1 b - - 0 1"))
    fen_assert("Rc7",  evaluate(None, "1r2r1k1/3bnppp/p2q4/2RPp3/4P3/6P1/2Q1NPBP/2R3K1 w - - 0 1"))
    fen_assert("O-O",  evaluate(None, "2b1k2r/2p2ppp/1qp4n/7B/1p2P3/5Q2/PPPr2PP/R2N1R1K b k - 0 1"))
    fen_assert("Rd8",  evaluate(None, "2b5/1p4k1/p2R2P1/4Np2/1P3Pp1/1r6/5K2/8 w - - 0 1"))
    fen_assert("g3",  evaluate(None, "2brr1k1/ppq2ppp/2pb1n2/8/3NP3/2P2P2/P1Q2BPP/1R1R1BK1 w - - 0 1"))
    fen_assert("Bf7",  evaluate(None, "2kr2nr/1pp3pp/p1pb4/4p2b/4P1P1/5N1P/PPPN1P2/R1B1R1K1 b - - 0 1"))
    fen_assert("O-O",  evaluate(None, "2r1k2r/1p1qbppp/p3pn2/3pBb2/3P4/1QN1P3/PP2BPPP/2R2RK1 b k - 0 1"))
    fen_assert("Rce1",  evaluate(None, "2r1r1k1/pbpp1npp/1p1b3q/3P4/4RN1P/1P4P1/PB1Q1PB1/2R3K1 w - - 0 1"))
    fen_assert("Kd2",  evaluate(None, "2r2k2/r4p2/2b1p1p1/1p1p2Pp/3R1P1P/P1P5/1PB5/2K1R3 w - - 0 1"))
    fen_assert("Rd1",  evaluate(None, "2r3k1/5pp1/1p2p1np/p1q5/P1P4P/1P1Q1NP1/5PK1/R7 w - - 0 1"))
    fen_assert("e6",  evaluate(None, "2r3qk/p5p1/1n3p1p/4PQ2/8/3B4/5P1P/3R2K1 w - - 0 1"))
    fen_assert("Qf3",  evaluate(None, "3b4/3k1pp1/p1pP2q1/1p2B2p/1P2P1P1/P2Q3P/4K3/8 w - - 0 1"))
    fen_assert("Na4",  evaluate(None, "3n1r1k/2p1p1bp/Rn4p1/6N1/3P3P/2N1B3/2r2PP1/5RK1 w - - 0 1"))
    fen_assert("Nd4",  evaluate(None, "3q1rk1/3rbppp/ppbppn2/1N6/2P1P3/BP6/P1B1QPPP/R3R1K1 w - - 0 1"))
    fen_assert("Qd6",  evaluate(None, "3r2k1/2q2p1p/5bp1/p1pP4/PpQ5/1P3NP1/5PKP/3R4 b - - 0 1"))
    fen_assert("Kd7",  evaluate(None, "3r4/2k5/p3N3/4p3/1p1p4/4r3/PPP3P1/1K1R4 b - - 0 1"))
    fen_assert("b4",  evaluate(None, "3r4/2R1np1p/1p1rpk2/p2b1p2/8/PP2P3/4BPPP/2R1NK2 w - - 0 1"))
    fen_assert("Nf5",  evaluate(None, "3rkb1r/pppb1pp1/4n2p/2p5/3NN3/1P5P/PBP2PP1/3RR1K1 w - - 0 1"))
    fen_assert("Rfe1",  evaluate(None, "3rr1k1/1pq2ppp/p1n5/3p4/6b1/2P2N2/PP1QBPPP/3R1RK1 w - - 0 1"))
    fen_assert("Rc1",  evaluate(None, "4r1k1/1q1n1ppp/3pp3/rp6/p2PP3/N5P1/PPQ2P1P/3RR1K1 w - - 0 1"))
    fen_assert("Qe2",  evaluate(None, "4rb1k/1bqn1pp1/p3rn1p/1p2pN2/1PP1p1N1/P1P2Q1P/1BB2PP1/3RR1K1 w - - 0 1"))
    fen_assert("Rfe1",  evaluate(None, "4rr2/2p5/1p1p1kp1/p6p/P1P4P/6P1/1P3PK1/3R1R2 w - - 0 1"))
    fen_assert("Ke7",  evaluate(None, "5r2/pp1b1kpp/8/2pPp3/2P1p2P/4P1r1/PPRKB1P1/6R1 b - - 0 1"))
    fen_assert("Nf6+",  evaluate(None, "6k1/1R5p/r2p2p1/2pN2B1/2bb4/P7/1P1K2PP/8 w - - 0 1"))
    fen_assert("Qc5",  evaluate(None, "6k1/pp1q1pp1/2nBp1bp/P2pP3/3P4/8/1P2BPPP/2Q3K1 w - - 0 1"))
    fen_assert("Rd2",  evaluate(None, "6k1/pp2rp1p/2p2bp1/1n1n4/1PN3P1/P2rP2P/R3NPK1/2B2R2 w - - 0 1"))
    fen_assert("h4",  evaluate(None, "8/2p2kpp/p6r/4Pp2/1P2pPb1/2P3P1/P2B1K1P/4R3 w - - 0 1"))
    fen_assert("Kh7",  evaluate(None, "Q5k1/5pp1/5n1p/2b2P2/8/5N1P/5PP1/2q1B1K1 b - - 0 1"))
    fen_assert("O-O",  evaluate(None, "r1b1k2r/ppppqppp/8/2bP4/3p4/6P1/PPQPPPBP/R1B2RK1 b kq - 0 1"))
    fen_assert("O-O",  evaluate(None, "r1b1k2r/ppppqppp/8/2bP4/3p4/6P1/PPQPPPBP/R1B2RK1 b kq - 0 1"))
    fen_assert("O-O",  evaluate(None, "r1b1k2r/ppq1bppp/2n5/2N1p3/8/2P1B1P1/P3PPBP/R2Q1RK1 b kq - 0 1"))
    fen_assert("O-O",  evaluate(None, "r1b1kb1r/pp2qppp/2pp4/8/4nP2/2N2N2/PPPP2PP/R1BQK2R w KQkq - 0 1"))
    fen_assert("Rd3",  evaluate(None, "r1b1qrk1/pp4b1/2pRn1pp/5p2/2n2B2/2N2NPP/PPQ1PPB1/5RK1 w - - 0 1"))
    fen_assert("Rc1",  evaluate(None, "r1b2rk1/1pqn1pp1/p2bpn1p/8/3P4/2NB1N2/PPQB1PPP/3R1RK1 w - - 0 1"))
    fen_assert("Qh6",  evaluate(None, "r1b2rk1/2qnbp1p/p1npp1p1/1p4PQ/4PP2/1NNBB3/PPP4P/R4RK1 w - - 0 1"))

if __name__ == "__main__":
    main()
