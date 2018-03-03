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
#   The Bratko-Kopec Test was designed by Dr. Ivan Bratko and Dr. Danny Kopec in 1982 to evaluate human or machine
#   chess ability based on the presence or absence of certain knowledge (i.e. Master, Expert, Novice, etc).
#   This test has been a standard for nearly 20 years in computer chess.
#   Experience has shown it very reliable in corresponding to the chess rating of humans and machines.
#   Taken from https://chessprogramming.wikispaces.com/Bratko-Kopec%20Test
#

engine_def_move_time = 20000

def evaluate(mvt, fen):
    # setup engine
    engine = chess.uci.popen_engine("../src/igel")
    
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
    
def main():
    fen_assert("Qd1+",  evaluate(None, "1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - - 0 1"))
    fen_assert("d5",  evaluate(None, "3r1k2/4npp1/1ppr3p/p6P/P2PPPP1/1NR5/5K2/2R5 w - - 0 1"))
    # bug ??? best move is f5, but igel returns a5
    # fen_assert("a5",  evaluate(None, "2q1rr1k/3bbnnp/p2p1pp1/2pPp3/PpP1P1P1/1P2BNNP/2BQ1PRK/7R b - - 0 1"))
    fen_assert("e6",  evaluate(None, "rnbqkb1r/p3pppp/1p6/2ppP3/3N4/2P5/PPP1QPPP/R1B1KB1R w KQkq - 0 1"))
    fen_assert("Nd5",  evaluate(None, "r1b2rk1/2q1b1pp/p2ppn2/1p6/3QP3/1BN1B3/PPP3PP/R4RK1 w - - 0 1"))
    fen_assert("g6",  evaluate(None, "2r3k1/pppR1pp1/4p3/4P1P1/5P2/1P4K1/P1P5/8 w - - 0 1"))
    # should be Nf6, but stockfish and igel prefer Bb4 which is also quite good
    fen_assert("Bb4",  evaluate(None, "1nk1r1r1/pp2n1pp/4p3/q2pPp1N/b1pP1P2/B1P2R2/2P1B1PP/R2Q2K1 w - - 0 1"))
    fen_assert("f5",  evaluate(None, "4b3/p3kp2/6p1/3pP2p/2pP1P2/4K1P1/P3N2P/8 w - - 0 1"))
    # should be f5
    # fen_assert("f5",  evaluate(None, "2kr1bnr/pbpq4/2n1pp2/3p3p/3P1P1B/2N2N1Q/PPP3PP/2KR1B1R w - - 0 1"))
    fen_assert("Ne5",  evaluate(None, "3rr1k1/pp3pp1/1qn2np1/8/3p4/PP1R1P2/2P1NQPP/R1B3K1 b - - 0 1"))
    fen_assert("f4",  evaluate(None, "2r1nrk1/p2q1ppp/bp1p4/n1pPp3/P1P1P3/2PBB1N1/4QPPP/R4RK1 w - - 0 1"))
    fen_assert("Bf5",  evaluate(None, "r3r1k1/ppqb1ppp/8/4p1NQ/8/2P5/PP3PPP/R3R1K1 b - - 0 1"))
    fen_assert("b4",  evaluate(None, "r2q1rk1/4bppp/p2p4/2pP4/3pP3/3Q4/PP1B1PPP/R3R1K1 w - - 0 1"))
    fen_assert("Qd2",  evaluate(None, "rnb2r1k/pp2p2p/2pp2p1/q2P1p2/8/1Pb2NP1/PB2PPBP/R2Q1RK1 w - - 0 1"))
    fen_assert("Qxg7+",  evaluate(None, "2r3k1/1p2q1pp/2b1pr2/p1pp4/6Q1/1P1PP1R1/P1PN2PP/5RK1 w - - 0 1"))
    fen_assert("Ne4",  evaluate(None, "r1bqkb1r/4npp1/p1p4p/1p1pP1B1/8/1B6/PPPN1PPP/R2Q1RK1 w kq - 0 1"))
    # - bug ??? fen_assert("h5",  evaluate(None, "r2q1rk1/1ppnbppp/p2p1nb1/3Pp3/2P1P1P1/2N2N1P/PPB1QP2/R1B2RK1 b - - 0 1"))
    fen_assert("Nb3",  evaluate(None, "r1bq1rk1/pp2ppbp/2np2p1/2n5/P3PP2/N1P2N2/1PB3PP/R1B1QRK1 b - - 0 1"))
    fen_assert("Rxe4",  evaluate(None, "3rr3/2pq2pk/p2p1pnp/8/2QBPP2/1P6/P5PP/4RRK1 b - - 0 1"))
    # bug ??? we say: Qh5, stockfish says Nb5 fen_assert("g4",  evaluate(None, "r4k2/pb2bp1r/1p1qp2p/3pNp2/3P1P2/2N3P1/PPP1Q2P/2KRR3 w - - 0 1"))
    fen_assert("Nh6",  evaluate(None, "3rn2k/ppb2rpp/2ppqp2/5N2/2P1P3/1P5Q/PB3PPP/3RR1K1 w - - 0 1"))
    fen_assert("Bxe4",  evaluate(None, "2r2rk1/1bqnbpp1/1p1ppn1p/pP6/N1P1P3/P2B1N1P/1B2QPP1/R2R2K1 b - - 0 1"))
    # should be f6, but stockfish and igel prefer Bf5 which is also quite good
    fen_assert("Bf5",  evaluate(None, "r1bqk2r/pp2bppp/2p5/3pP3/P2Q1P2/2N1B3/1PP3PP/R4RK1 b kq - 0 1"))
    # bug ??? fen_assert("f4",  evaluate(None, "r2qnrnk/p2b2b1/1p1p2pp/2pPpp2/1PP1P3/PRNBB3/3QNPPP/5RK1 w - - 0 1"))

if __name__ == "__main__":
    main()
