/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2018 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
*
*  Igel is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  Igel is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with Igel.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../moves.h"
#include "../position.h"
#include "../eval.h"
#include "../utils.h"
#include "../notation.h"

#include <functional>
#include <string>

#include <gtest/gtest.h>

namespace unit
{

NODES Perft(Position & pos, int depth)
{
    if (depth == 0)
        return 1;

    NODES total = 0;
    MoveList mvlist;

    EXPECT_EQ(true, GenAllMoves(pos, mvlist));

    auto mvSize = mvlist.Size();
    for (size_t i = 0; i < mvSize; ++i)
    {
        Move mv = mvlist[i].m_mv;
        EXPECT_LE(mv, 16777215); // move must be encoded as 24 bit max
        if (pos.MakeMove(mv))
        {
            total += Perft(pos, depth - 1);
            pos.UnmakeMove();
        }
    }

    return total;
}

TEST(MoveGen, Positive)
{
    Position pos;
    pos.SetInitial();

    // taken from https://chessprogramming.wikispaces.com/Perft%20Results
    EXPECT_EQ(1, Perft(pos, 0));
    EXPECT_EQ(20, Perft(pos, 1));
    EXPECT_EQ(400, Perft(pos, 2));
    EXPECT_EQ(8902, Perft(pos, 3));
    EXPECT_EQ(197281, Perft(pos, 4));
    EXPECT_EQ(4865609, Perft(pos, 5));
    EXPECT_EQ(119060324, Perft(pos, 6));
}

TEST(Move, Negative)
{
    MoveList mvlist;
    EXPECT_EQ(false, mvlist.Swap(256, 256));
    EXPECT_EQ(false, mvlist.Swap(257, 256));
    EXPECT_EQ(false, mvlist.Swap(250, 256));
}

TEST(Move, Positive)
{
    MoveList mvlist;
    EXPECT_EQ(0, mvlist.Size());

    Move mv;
    EXPECT_EQ(0, mv);

    for (auto i = 1; i < 256; ++i)
    {
        Move m = i;
        EXPECT_EQ(true, mvlist.Add(m));
        EXPECT_EQ(i, mvlist.Size());
    }

    for (auto i = 0; i < 255; ++i)
    {
        Move m = i + 1;
        EXPECT_EQ(m, mvlist[i].m_mv);
    }

    for (auto i = 1; i < 256; ++i)
    {
        EXPECT_EQ(false, mvlist.Add(mv));
        EXPECT_EQ(255, mvlist.Size());
    }

    mvlist.Clear();
    EXPECT_EQ(0, mvlist.Size());

    mv = 1;
    EXPECT_EQ(true, mvlist.Add(mv));
    mv = 2;
    EXPECT_EQ(true, mvlist.Add(mv));

    EXPECT_EQ(1, mvlist[0].m_mv);
    EXPECT_EQ(2, mvlist[1].m_mv);

    EXPECT_EQ(true, mvlist.Swap(0, 1));
    EXPECT_EQ(2, mvlist[0].m_mv);
    EXPECT_EQ(1, mvlist[1].m_mv);
}

NODES PerftChecks(Position & pos, int depth)
{
    NODES checks = 0;

    if (depth == 0)
        return checks;

    MoveList mvlist;

    EXPECT_EQ(true, GenAllMoves(pos, mvlist));

    for (size_t i = 0; i < mvlist.Size(); ++i)
    {
        Move mv = mvlist[i].m_mv;
        EXPECT_LE(mv, 16777215); // move must be encoded as 24 bit max

        if (pos.MakeMove(mv))
        {
            if (pos.InCheck())
                ++checks;
            else
                checks += PerftChecks(pos, depth - 1);
            pos.UnmakeMove();
        }
    }

    return checks;
}

void ValidateMoveGen(const char * fen, const char * expectedMoves, std::function<void(const Position&, MoveList&)> mvgen)
{
    Position position;
    EXPECT_EQ(true, position.SetFEN(fen));

    MoveList moves;
    mvgen(position, moves);

    std::string s = expectedMoves;
    std::string delimiter = ",";
    size_t pos = 0;
    std::string token;
    std::set<std::string> expMoves;

    while (true)
    {
        pos = s.find(delimiter); 
        
        if (pos != std::string::npos)
        {
            token = s.substr(0, pos);
            expMoves.insert(token);
            s.erase(0, pos + delimiter.length());
        }
        else
        {
            if (!s.empty())
                expMoves.insert(s);
            break;
        }
    }

    EXPECT_EQ(expMoves.size(), moves.Size());

    for (size_t i = 0; i < moves.Size(); ++i)
    {
        std::string move = MoveToStrLong(moves[i].m_mv);

        EXPECT_EQ(true, expMoves.find(move) != expMoves.end());
        expMoves.erase(move);
    }

    EXPECT_EQ(0, expMoves.size());
}

TEST(MoveGenChecks, Positive)
{
    Position pos;
    pos.SetInitial();

    // taken from https://chessprogramming.wikispaces.com/Perft%20Results
    EXPECT_EQ(0, PerftChecks(pos, 0));
    EXPECT_EQ(0, PerftChecks(pos, 1));
    EXPECT_EQ(0, PerftChecks(pos, 2));
    EXPECT_EQ(12, PerftChecks(pos, 3));
}

TEST(SimpleChecksKnights, Positive)
{
    ValidateMoveGen("4k3/8/8/8/3n4/8/8/4K3 b KQkq -", "d4c2,d4f3", AddSimpleChecks);
    ValidateMoveGen("4k3/8/8/8/3n4/8/2P5/4K3 b - -", "d4f3", AddSimpleChecks);
    ValidateMoveGen("4k3/8/8/8/8/n7/8/K7 b KQkq -", "a3c2", AddSimpleChecks);
    ValidateMoveGen("r1b1kbnr/ppp2ppp/8/2np4/4Pp2/5N2/PPPP2PP/RNBQKB1R b KQkq -", "c5d3", AddSimpleChecks);
    ValidateMoveGen("r1bqkb1r/pppp1ppp/8/4n3/3pPn2/2P5/PP3P1P/RNBQKBNR b KQkq -", "e5d3,e5f3,f4d3,f4g2", AddSimpleChecks);
    ValidateMoveGen("r1bqkb1r/pppp1ppp/8/4n3/1n1pPn2/2P5/PP3P1P/RNBQKBNR b KQkq -", "e5d3,e5f3,f4d3,f4g2,b4d3,b4c2", AddSimpleChecks);
    ValidateMoveGen("r1bqkb1r/pppp1ppp/8/4n3/1n1pPn2/2P5/PPN2P1P/R1BQKBNR b KQkq -", "e5d3,e5f3,f4d3,f4g2,b4d3", AddSimpleChecks);
}

TEST(SimpleChecksBishops, Positive)
{
    ValidateMoveGen("rnbqkb1r/ppp1pp1p/5np1/3p4/2PP4/2N5/PPB1PPPP/R1BQK1NR w KQkq -", "c2a4", AddSimpleChecks);
    ValidateMoveGen("rnbqkb1r/ppp1p2p/5n2/3p4/2PP4/2N5/PPB1PPPP/R1BQK1NR w KQkq -", "c2a4,c2g6", AddSimpleChecks);
    ValidateMoveGen("rnbqkb1r/ppp1p2p/4Bn2/3p4/2PP4/2N5/PPB1PPPP/R1BQK1NR w KQkq -", "c2a4,c2g6,e6d7,e6f7", AddSimpleChecks);
    ValidateMoveGen("rnbqkb1r/pppRpn1p/4Bn2/3p4/2PP4/2N5/PPB1PPPP/R1BQK1NR w KQkq -", "", AddSimpleChecks);
}

TEST(SimpleChecksRooks, Positive)
{
    ValidateMoveGen("rnbqk2r/pppp1p1p/4pn2/8/1bPP4/2N3R1/PP2PPPP/R1BQKBN1 w KQkq -", "g3g8", AddSimpleChecks);
    ValidateMoveGen("rnbqk2r/pppp1pRp/4p3/8/1bPP4/2N3R1/P3PPPP/2BQKBN1 w KQkq -", "g7g8", AddSimpleChecks);
    ValidateMoveGen("rnbqk2r/pppp1RRp/4p3/8/1bPP4/2N3R1/P3PPPP/2BQKBN1 w KQkq -", "g7g8,f7f8,f7e7", AddSimpleChecks);

    // special case: checking via long castling, currently not supported
    ValidateMoveGen("rnbq1b1r/pppkpppp/5n2/8/8/8/PPP2PPP/R3K1NR w KQ -", "a1d1", AddSimpleChecks);
}

TEST(SimpleChecksByPawns, Positive)
{
    ValidateMoveGen("8/8/8/4k3/8/3p4/8/4K3 b KQkq -", "d3d2", AddSimpleChecks);
    ValidateMoveGen("k7/8/8/8/p7/p7/8/1K6 b - -", "a3a2", AddSimpleChecks);
    ValidateMoveGen("k7/8/8/8/p7/p7/P7/1K6 b - -", "", AddSimpleChecks);
    ValidateMoveGen("k7/8/8/8/p7/p7/R7/1K6 b - -", "", AddSimpleChecks);
}

TEST(SimpleChecksQueens, Positive)
{
    ValidateMoveGen("r1bqkbnr/ppp2ppp/8/3p4/4Pp2/5N2/PPPP2PP/RNBQKB1R b KQkq -", "d8h4", AddSimpleChecks);
    ValidateMoveGen("rnb1kbnr/pppp1ppp/8/8/3pP3/2Pq1P2/PP4PP/RNBQKBNR b KQkq -", "d3d2,d3e3,d3e2", AddSimpleChecks);
}

TEST(SimpleChecksComposite, Positive)
{
    ValidateMoveGen("rnbq1b1r/pppkpppp/5n2/4P3/4N3/2Q5/1PP2PP1/R3KB1R w KQ -", "e5e6,e4c5,a1d1,c3c6,c3d4,c3d3,c3d2,c3h3,f1b5", AddSimpleChecks);
}

TEST(CapturesAndPromotionsPawns, Positive)
{
    // captures
    ValidateMoveGen("rnbq1b1r/pppkpppp/5n2/4P3/8/2N5/1PP2PP1/1QR1KBB1 w KQ -", "e5f6", GenCapturesAndPromotions);
    ValidateMoveGen("rnbq3r/pppkpppp/3b1n2/4P3/8/2N5/1PP2PP1/1QR1KBB1 w KQ -", "e5f6,e5d6", GenCapturesAndPromotions);
    ValidateMoveGen("rnbqr3/pppk1ppp/3b1n2/4P3/8/2N5/1PP2PP1/1QR1KBB1 w KQ -", "e5f6,e5d6", GenCapturesAndPromotions);
    ValidateMoveGen("rnbqr3/pppk1p1p/3bqn2/4PP2/8/2N5/1PP2P2/1QR1KBB1 w KQ -", "e5f6,e5d6,f5e6", GenCapturesAndPromotions);

    // promotions
    ValidateMoveGen("rnbqr3/p1pk1pPp/3bqn2/8/8/2N5/1PP2PP1/1QR1KBB1 w KQ -", "g7g8q,g7g8r,g7g8b,g7g8n", GenCapturesAndPromotions);
    ValidateMoveGen("rnbqr3/p1pk1pPp/3bqn2/4PP2/8/2N5/1PP2PP1/1QR1KBB1 w KQ -", "e5f6,e5d6,f5e6,g7g8q,g7g8r,g7g8b,g7g8n", GenCapturesAndPromotions);
    ValidateMoveGen("rnbqr1b1/p1pk1pPp/3bqn2/4PP2/8/2N5/1PP2PP1/1QR1KBB1 w KQ -", "e5f6,e5d6,f5e6", GenCapturesAndPromotions);
    ValidateMoveGen("rnbqrnbq/p1pk1pPp/3bqn2/8/8/2N5/1PP2PP1/1QR1KBB1 w KQ -", "g7f8q,g7f8r,g7f8b,g7f8n,g7h8q,g7h8r,g7h8b,g7h8n", GenCapturesAndPromotions);
}

TEST(CapturesKnights, Positive)
{
    ValidateMoveGen("rnbqkb1r/ppp1pppp/5n2/3p4/3P2N1/2N5/PPP1PPPP/R1BQKB1R w KQkq -", "c3d5,g4f6", GenCapturesAndPromotions);
    ValidateMoveGen("r1bqkb1r/pppp1ppp/2n2n2/3N2N1/3PP3/5N2/PPP2PPP/R1BQKB1R w KQkq -", "d5f6,d5c7,g5f7,g5h7", GenCapturesAndPromotions);
    ValidateMoveGen("r1bqkb1r/pppp1ppp/2n2n2/3N2N1/3PP3/8/PPP2PPP/R1BQKB1R w KQkq -", "d5f6,d5c7,g5f7,g5h7", GenCapturesAndPromotions);
    ValidateMoveGen("rnbqkbnr/pp2pppp/2p4N/N7/3P4/8/PPP2PPP/R1BQKB1R w KQkq -", "a5c6,a5b7,h6f7,h6g8", GenCapturesAndPromotions);
}

TEST(CapturesBishops, Positive)
{
    ValidateMoveGen("rn2k2r/pp3ppp/4pn2/2b5/2PP4/2N5/PP2bPPP/R1BQKBNR b KQkq -", "e2f1,e2d1,e2c4,c5d4", GenCapturesAndPromotions);
    ValidateMoveGen("rn2k2r/pp3ppp/1b2pn2/2b5/2PP2b1/2N5/PP2bPPP/R1BQKBNR b KQkq -", "e2f1,e2d1,e2c4,c5d4", GenCapturesAndPromotions);
    ValidateMoveGen("rn2k2r/pp3ppp/1b2pn2/2b5/2PP4/2N2b2/PP2bPPP/R1BQKBNR b KQkq -", "e2f1,e2d1,e2c4,c5d4,f3g2", GenCapturesAndPromotions);
    ValidateMoveGen("rn2k2r/pp3ppp/1b2pn2/2b5/b1PP4/1NN2b2/PP2bPPP/R1BQKB1R b KQkq -", "e2f1,e2d1,e2c4,c5d4,f3g2,a4b3", GenCapturesAndPromotions);
}

TEST(CapturesRooks, Positive)
{
    ValidateMoveGen("rnbqk1nr/pppp1ppp/8/2bRp3/4P3/8/PPPP2PP/RNBQKBN1 w KQkq -", "d5e5,d5c5,d5d7", GenCapturesAndPromotions);
    ValidateMoveGen("rnbqk1nr/pppp1ppp/8/1qPRp3/4P3/8/PPPP2PP/RNBQK1N1 w KQkq -", "d5e5,d5d7", GenCapturesAndPromotions);
    ValidateMoveGen("rnbqk2r/pppp1ppp/8/1qPRp3/4P3/1R4n1/PPPP2P1/RNBQK1N1 w KQkq -", "d5e5,d5d7,b3b5,b3g3", GenCapturesAndPromotions);
    ValidateMoveGen("rnbqk2r/pppp1ppp/8/1qPRp3/4P3/1R3Pn1/PPPP2P1/RNBQK1N1 w KQkq -", "d5e5,d5d7,b3b5", GenCapturesAndPromotions);
}

TEST(CapturesQueen, Positive)
{
    ValidateMoveGen("r1bqkb2/ppp1pppp/5n2/8/1n1Q3r/8/PPP2PPP/RNB1KBNR w KQkq -", "d4b4,d4h4,d4f6,d4d8,d4a7", GenCapturesAndPromotions);
    ValidateMoveGen("r1bqkb2/ppp1pppp/5n2/8/1n1QQ2r/8/PPP2PPP/RNB1KBNR w KQkq -", "d4b4,d4f6,d4d8,d4a7,e4b7,e4e7,e4h7,e4h4", GenCapturesAndPromotions);
    ValidateMoveGen("r1bqkb1Q/ppp1pppp/5n2/8/1n1QQ2r/8/PPP2PPP/RNB1KBNR w KQkq -", "d4b4,d4f6,d4d8,d4a7,e4b7,e4e7,e4h7,e4h4,h8f8,h8g7,h8h7", GenCapturesAndPromotions);
}

TEST(CapturesComposite, Positive)
{
    ValidateMoveGen("rnb1kb1r/ppp2pp1/B3pn2/1q1PNNB1/1Q1P3p/6P1/PPP2P1P/R3K2R w KQkq -", "a6b5,a6b7,b4b5,b4f8,d5e6,e5f7,f5g7,g5f6,g5h4,g3h4,f5h4", GenCapturesAndPromotions);
}

}
