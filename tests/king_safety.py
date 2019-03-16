#
#  Igel - a UCI chess playing engine derived from GreKo 2018.01
#
#  Copyright (C) 2018-2019 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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
import chess.uci
import timeit

engine_def_move_time = 1000

def evaluate(mate,  fen):
    # setup engine
    engine = chess.uci.popen_engine("../igel")
    engine.setoption({"Threads": 2})

    # setup board
    board = chess.Board(fen)
    handler = chess.uci.InfoHandler()
    print(fen)
    print(board)

    engine_move_time = engine_def_move_time
        
    # run engine
    engine.info_handlers.append(handler)
    engine.ucinewgame()
    engine.position(board)
    start_time = timeit.default_timer()
    bestmove, pondermove = engine.go(movetime=engine_move_time)
    elapsed = timeit.default_timer() - start_time
    elapsed = elapsed * 1000
    if elapsed > (engine_move_time + 200):
        engine.quit()
        raise Exception("exceeded a go time", elapsed)
    print (handler.info["score"][1])
    print("search time: %d ms" % elapsed)
    if mate != handler.info["score"][1].mate:
        raise Exception(mate, handler.info["score"][1].mate)
    engine.quit()

    return handler.info["score"][1].mate

def mate_assert(engine_move,  expected_move):
    if engine_move != expected_move:
            raise Exception(engine_move, expected_move)
    
def main():

    #
    #   Chess positions taken from http://www.talkchess.com/forum/viewtopic.php?topic_view=threads&p=665516&t=59647
    #

    mate_assert(-2,  evaluate(-2, "2b2q1k/5P1p/p7/1p1pB3/3P4/1P6/5PP1/6K1 b - - 0 1"))
    mate_assert(-2,  evaluate(-2, "1q5r/pp3N2/n3rp1k/2P4p/3PP3/PP6/1K5R/6R1 b - - 0 1"))
    mate_assert(-2,  evaluate(-2, "1r6/5Q2/4p2k/1n1pP1P1/2bP2P1/6K1/q4P2/4N3 b - - 0 1"))
    mate_assert(-2,  evaluate(-2, "r1b5/1p1n4/p3q1r1/4p3/7Q/5P2/P2Bp1kR/K7 b - - 0 1"))
    mate_assert(-2,  evaluate(-2, "5r1r/1bqnQp2/1pp1n3/p6p/P1PpPk1N/3P3P/P5P1/1RB3K1 b - -  0 1"))
    mate_assert(-2,  evaluate(-2, "1k3N2/1p4Q1/p4N2/1b6/1q6/3r3P/3p2P1/1K1R3R w - - 0 1"))
    mate_assert(-2,  evaluate(-2, "r1b5/1p3p2/p3p1pp/6P1/7k/1P4QP/P4P2/2q2BK1 b - - 0 1"))
    mate_assert(-2,  evaluate(-2, "3rb2r/NRk5/4p1p1/p4pq1/2PP4/P7/P4PB1/4R1K1 b - - 0 1"))
    mate_assert(-2,  evaluate(-2, "1r1kbb2/1n4R1/p3N3/1p5r/7p/2B4P/1P3PP1/6K1 b - - 0 1"))
    mate_assert(-2,  evaluate(-2, "rr3k2/qb2b1pB/2p1Nnp1/3pN3/3P1P2/4P2P/P5P1/1R3RK1 b - - 0 1"))
    mate_assert(-2,  evaluate(-2, "r1b5/pp3p2/n3Bq1b/1Np5/2Pp1kR1/3P4/PP2PP2/R3K3 b Q - 0 1"))
    mate_assert(-2,  evaluate(-2, "2r1r2k/2qN1B2/p1b1pBPb/PpPp4/1P3P2/3R4/8/6K1 b - - 0 1"))
    mate_assert(-2,  evaluate(-2, "2r3k1/3n2p1/2pbNn1p/3pNp1P/3P4/1pQPP3/qP1B1PP1/1K1R3R w - - 0 1"))
    mate_assert(-2,  evaluate(-2, "q3brk1/2n2p2/rp2pB2/2b5/P1P1p3/4PB2/3N1P2/R3K2R b KQ - 0 1"))
    mate_assert(-2,  evaluate(-2, "1kN2b1r/Qprbqp2/p4npp/4p3/P3P3/3P3P/1PP3P1/R4RK1 b - - 0 1"))
    mate_assert(-2,  evaluate(-2, "3r4/4qp1Q/r3p3/3pP1k1/PpPR3p/1P3PpP/8/6K1 b - - 0 1"))
    mate_assert(-2,  evaluate(-2, "q3r3/1b5N/1p3Q2/3p4/PPp4k/3n4/3P1PP1/3K4 b - - 0 1"))
    mate_assert(-2,  evaluate(-2, "2r3k1/7R/5BK1/6p1/8/p5rp/8/8 b - - 0 1"))
    mate_assert(-2,  evaluate(-2, "3R2k1/prp2r1p/4N1p1/4qp2/4p3/1P2P3/P1R2PPP/6K1 b - - 0 1"))
    mate_assert(-2,  evaluate(-2, "7r/6R1/4pP1k/1p2nN1p/1P5N/8/3q2P1/6K1 b - - 0 1"))
    mate_assert(-2,  evaluate(-2, "8/2Q3pk/1N5p/4P3/8/5qr1/7R/2R3K1 w - - 0 1"))
    mate_assert(-2,  evaluate(-2, "rn3Q2/1qp3p1/pp3k1p/2r1b3/5P1N/P5P1/1P5P/3R2K1 b - - 0 1"))
    mate_assert(-2,  evaluate(-2, "3Q4/5Rpk/4p2p/p1p1P3/P4B2/2r5/2q5/1K1R4 w - - 0 1"))
    mate_assert(-2,  evaluate(-2, "r1q2r1k/1b2np2/p2p1P1Q/1p2p3/4P3/P7/1PP1BP2/1K1R4 b - - 0 1"))
    mate_assert(-2,  evaluate(-2, "8/7k/5NR1/p3pP2/P3P3/6K1/4r3/1q6 b - - 0 1"))
    mate_assert(-2,  evaluate(-2, "1r1b2kR/3q4/1p6/p1r1pP2/P1P1N1P1/3P2K1/8/7R b - - 0 1"))
    mate_assert(-2,  evaluate(-2, "r3r1k1/1pq1bp1p/p2p1B2/2p5/P3P3/1P4Q1/2PP3P/2K5 b - - 0 1"))
    mate_assert(-2,  evaluate(-2, "2k3rr/2P5/8/5p2/pP2pb2/P2P3Q/1R3P1K/N2R4 w - - 0 1"))
    mate_assert(-2,  evaluate(-2, "6k1/4r3/n1pb1n2/p2p4/1P1P1p2/1PB1pPrK/N1P1B3/R2QR3 w - - 0 1"))
    mate_assert(-2,  evaluate(-2, "8/1pp1kp2/8/2r3r1/4B2K/Q3P2R/PP3q2/R1B5 w - - 0 1"))
    mate_assert(-2,  evaluate(-2, "2b1k2r/1p1pq2p/p3pK2/2b2pp1/P1P5/1Q2P1B1/5PPP/R4BNR w k - 0 1"))
    mate_assert(-2,  evaluate(-2, "6k1/1p2ppb1/1Q1N1n1p/5b1P/2r3p1/2B2P2/1Kq1P1P1/3R1BNR w - - 0 1"))
    mate_assert(-2,  evaluate(-2, "7k/6pp/8/8/NQR4P/P5R1/5r1K/5q2 w - - 0 1"))
    mate_assert(-2,  evaluate(-2, "1rb5/7B/2p4Q/4q3/2PP2Pk/7P/r4P2/6K1 b - - 0 1"))
    mate_assert(-2,  evaluate(-2, "n1k4r/3Qnpp1/4p2p/2N1P2P/8/8/Pq3PP1/6K1 b - - 0 1"))
    mate_assert(-2,  evaluate(-2, "7k/PR1Q2p1/2R2p2/8/3P1PPp/n6P/8/3qr1K1 w - - 0 1"))
    mate_assert(-2,  evaluate(-2, "rB5k/p4Qp1/4p2p/1K6/1qPNb3/3n3P/5PP1/R4B1R w - - 0 1"))
    mate_assert(-2,  evaluate(-2, "6k1/QR3p2/6pp/8/1BR1P1P1/5q1P/6K1/5r2 w - - 0 1"))

if __name__ == "__main__":
    main()
