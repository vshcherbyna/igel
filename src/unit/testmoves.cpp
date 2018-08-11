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

void ValidateChecksGen(const char * fen, const char * expectedMoves)
{
    Position position;
    position.SetFEN(fen);

    MoveList moves;
    AddSimpleChecks(position, moves);

    return;
    /*std::string s = expectedMoves;
    std::string delimiter = ";";
    size_t pos = 0;
    std::string token;
    std::set<std::string> expMoves;

    while ((pos = s.find(delimiter)) != std::string::npos)
    {
        token = s.substr(0, pos);
        expMoves.insert(token);
        s.erase(0, pos + delimiter.length());
    }

    cout << "tokens " << expMoves.size() << endl;*/
    //Position position;
    //position.SetInitial();

    //cout << "fen set to " << fen << endl;
    //position.SetFEN(fen);

    //cout << "fen set " << endl;

    //MoveList moves;
    //AddSimpleChecks(position, moves);

    /*EXPECT_EQ(expMoves.size(), moves.Size());

    cout << "sizes " << moves.Size() << endl;

    for (size_t i = 0; i < moves.Size(); ++i)
        EXPECT_EQ(true, expMoves.find(MoveToStrLong(moves[i].m_mv)) != expMoves.end());*/
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
    Position pos;
    pos.SetInitial();

    {
        MoveList mvs;
        AddSimpleChecks(pos, mvs); // we do not expect checks in initial position
        EXPECT_EQ(0, mvs.Size());
    }

    {
        MoveList mvs;
        // testing checks with knight in 7th rank
        pos.SetFEN("4k3/8/8/8/3n4/8/8/4K3 b KQkq -");
        AddSimpleChecks(pos, mvs);
        EXPECT_EQ(2, mvs.Size());
        EXPECT_EQ("d4c2", MoveToStrLong(mvs[0].m_mv));
        EXPECT_EQ("d4f3", MoveToStrLong(mvs[1].m_mv));
    }

    {
        // knight on the rim

        MoveList mvs;
        pos.SetFEN("4k3/8/8/8/8/n7/8/K7 b KQkq -");
        AddSimpleChecks(pos, mvs);
        EXPECT_EQ(1, mvs.Size());
        EXPECT_EQ("a3c2", MoveToStrLong(mvs[0].m_mv));
    }

    {
        // complicated opening

        MoveList mvs;
        pos.SetFEN("r1b1kbnr/ppp2ppp/8/2np4/4Pp2/5N2/PPPP2PP/RNBQKB1R b KQkq -");
        AddSimpleChecks(pos, mvs);

        for (size_t i = 0; i < mvs.Size(); ++i)
            cout << MoveToStrLong(mvs[i].m_mv) << endl;

        EXPECT_EQ(1, mvs.Size());
        EXPECT_EQ("c5d3", MoveToStrLong(mvs[0].m_mv));
    }

}

TEST(SimpleChecksByPawns, Positive)
{
    ValidateChecksGen("8/8/8/4k3/8/3p4/8/4K3 b KQkq -", "d3d2;");

    Position pos;

    MoveList mvs;
    pos.SetFEN("8/8/8/4k3/8/3p4/8/4K3 b KQkq -");

    AddSimpleChecks(pos, mvs);
    EXPECT_EQ(1, mvs.Size());
    EXPECT_EQ("d3d2", MoveToStrLong(mvs[0].m_mv));
}

TEST(SimpleChecksQueens, Positive)
{
    Position pos;
    pos.SetInitial();

    {
        // complicated opening

        MoveList mvs;
        pos.SetFEN("r1bqkbnr/ppp2ppp/8/3p4/4Pp2/5N2/PPPP2PP/RNBQKB1R b KQkq -");
        AddSimpleChecks(pos, mvs);
        EXPECT_EQ(1, mvs.Size());
        EXPECT_EQ("d8h4", MoveToStrLong(mvs[0].m_mv));
    }

    {
            // complicated opening

        MoveList mvs;
        pos.SetFEN("rnb1kbnr/pppp1ppp/8/8/3pP3/2Pq1P2/PP4PP/RNBQKBNR b KQkq -");
        AddSimpleChecks(pos, mvs);
        //cout << "queen checks " << mvs.Size() << endl;
        //for (size_t i = 0; i < mvs.Size(); ++i)
        //    cout << MoveToStrLong(mvs[i].m_mv) << endl;
        EXPECT_EQ(3, mvs.Size());
        //EXPECT_EQ("d8h4", MoveToStrLong(mvs[0].m_mv));
    }

}

}
