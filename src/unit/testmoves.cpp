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
#include <gtest/gtest.h>

namespace unit
{

NODES Perft(Position & pos, int depth)
{
    if (depth == 0)
        return 1;

    NODES total = 0;
    MoveList mvlist;

    GenAllMoves(pos, mvlist);

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
    std::unique_ptr<Position> pos(new Position);

    pos->SetInitial();

    // taken from https://chessprogramming.wikispaces.com/Perft%20Results
    EXPECT_EQ(1, Perft(*pos.get(), 0));
    EXPECT_EQ(20, Perft(*pos.get(), 1));
    EXPECT_EQ(400, Perft(*pos.get(), 2));
    EXPECT_EQ(8902, Perft(*pos.get(), 3));
    EXPECT_EQ(197281, Perft(*pos.get(), 4));
    EXPECT_EQ(4865609, Perft(*pos.get(), 5));
    EXPECT_EQ(119060324, Perft(*pos.get(), 6));
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
        mvlist.Add(m);
        EXPECT_EQ(i, mvlist.Size());
    }

    for (auto i = 0; i < 255; ++i)
    {
        Move m = i + 1;
        EXPECT_EQ(m, mvlist[i].m_mv);
    }

    mvlist.Clear();
    EXPECT_EQ(0, mvlist.Size());

    mv = 1;
    mvlist.Add(mv);
    mv = 2;
    mvlist.Add(mv);

    EXPECT_EQ(1, mvlist[0].m_mv);
    EXPECT_EQ(2, mvlist[1].m_mv);

    mvlist.Swap(0, 1);
    EXPECT_EQ(2, mvlist[0].m_mv);
    EXPECT_EQ(1, mvlist[1].m_mv);
}

NODES PerftChecks(Position & pos, int depth)
{
    NODES checks = 0;

    if (depth == 0)
        return checks;

    MoveList mvlist;

    GenAllMoves(pos, mvlist);

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

TEST(MoveGenChecks, Positive)
{
    std::unique_ptr<Position> pos(new Position);
    pos->SetInitial();

    // taken from https://chessprogramming.wikispaces.com/Perft%20Results
    EXPECT_EQ(0, PerftChecks(*pos, 0));
    EXPECT_EQ(0, PerftChecks(*pos, 1));
    EXPECT_EQ(0, PerftChecks(*pos, 2));
    EXPECT_EQ(12, PerftChecks(*pos, 3));
}

}
