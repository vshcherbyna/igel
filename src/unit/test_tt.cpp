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

#include "../tt.h"
#include "../moves.h"
#include "../eval.h"
#include <gtest/gtest.h>

namespace unit
{

TEST(TranspositionTableSizeTest, Negative)
{
    EXPECT_EQ(false, TTable::instance().setHashSize(0));
    EXPECT_EQ(false, TTable::instance().clearHash());
}

TEST(TranspositionTableSizeTest, Positive)
{
    for (auto i = 1; i <= 128; ++i)
        EXPECT_EQ(true, TTable::instance().setHashSize(i));

    EXPECT_EQ(true, TTable::instance().clearHash());
}

TEST(TranspositionTableEntryTest, Positive)
{
    for (auto i = 1; i <= 128; ++i)
    {
        EXPECT_EQ(true, TTable::instance().setHashSize(i));
        EXPECT_EQ(true, TTable::instance().record(i, 2, 3, 0, 1, i));

        auto * hentry = TTable::instance().retrieve(i);
        EXPECT_EQ(i, hentry->m_key ^ hentry->m_data);
        EXPECT_EQ(i, hentry->move());
        EXPECT_EQ(2, hentry->score());
        EXPECT_EQ(3, hentry->depth());
        EXPECT_EQ(1, hentry->type());
    }
}

TEST(TranspositionTableEntryScoreTest, Positive)
{
    TEntry te;

    EXPECT_EQ(0, te.m_data);
    EXPECT_EQ(0, te.m_key);

    te.store(0, 0, 0, 0, 0, 0);

    EXPECT_EQ(0, te.score());

    for (auto i = -INFINITY_SCORE; i <= INFINITY_SCORE; ++i)
    {
        te.store(0, i, 0, 0, 0, 0);
        EXPECT_EQ(i, te.score());
    }
}

TEST(TranspositionTableEntryDepthTest, Positive)
{
    TEntry te;

    EXPECT_EQ(0, te.m_data);
    EXPECT_EQ(0, te.m_key);

    te.store(0, 0, 0, 0, 0, 0);

    EXPECT_EQ(0, te.depth());

    for (auto i = 0; i < 255; ++i)
    {
        te.store(0, 0, i, 0, 0, 0);
        EXPECT_EQ(i, te.depth());
    }
}

TEST(TranspositionTableEntryMoveTest, Positive)
{
    TEntry te;

    EXPECT_EQ(0, te.m_data);
    EXPECT_EQ(0, te.m_key);

    te.store(0, 0, 0, 0, 0, 0);

    EXPECT_EQ(0, te.move());

    InitBitboards();
    Position::InitHashNumbers();
    InitEval();

    MoveList mvlist;
    Position pos;
    pos.SetInitial();

    EXPECT_EQ(true, GenAllMoves(pos, mvlist));

    auto mvSize = mvlist.Size();
    for (size_t i = 0; i < mvSize; ++i)
    {
        Move mv = mvlist[i].m_mv;
        te.store(mv, 0, 0, 0, 0, 0);
        EXPECT_EQ(mv, te.move());
    }
}

TEST(TranspositionTableEntryTypeTest, Positive)
{
    TEntry te;

    EXPECT_EQ(0, te.m_data);
    EXPECT_EQ(0, te.m_key);

    te.store(0, 0, 0, 0, 0, 0);
    EXPECT_EQ(0, te.type());

    for (auto i = 0; i < 3; ++i)
    {
        te.store(0, 0, 0, i, 0, 0);
        EXPECT_EQ(i, te.type());
    }
}

TEST(TranspositionTableEntryAgeTest, Positive)
{
    TEntry te;

    EXPECT_EQ(0, te.m_data);
    EXPECT_EQ(0, te.m_key);

    te.store(0, 0, 0, 0, 0, 0);
    EXPECT_EQ(0, te.age());

    for (auto i = 0; i < 255; ++i)
    {
        te.store(0, 0, 0, 0, 0, i);
        EXPECT_EQ(i, te.age());
    }
}

TEST(TranspositionTableEntryCompositeTest, Positive)
{
    TEntry te;

    EXPECT_EQ(0, te.m_data);
    EXPECT_EQ(0, te.m_key);

    te.store(0, 0, 0, 0, 0, 0);

    EXPECT_EQ(0, te.age());
    EXPECT_EQ(0, te.type());
    EXPECT_EQ(0, te.move());
    EXPECT_EQ(0, te.depth());
    EXPECT_EQ(0, te.score());

    te.store(1, 1, 1, 1, 1, 1);

    EXPECT_EQ(1, te.age());
    EXPECT_EQ(1, te.type());
    EXPECT_EQ(1, te.move());
    EXPECT_EQ(1, te.depth());
    EXPECT_EQ(1, te.score());

    te.store(16777215, INFINITY_SCORE, 1, 1, 1, 1);
    EXPECT_EQ(1, te.age());
    EXPECT_EQ(1, te.type());
    EXPECT_EQ(16777215, te.move());
    EXPECT_EQ(1, te.depth());
    EXPECT_EQ(INFINITY_SCORE, te.score());

    te.store(16777215, -INFINITY_SCORE, 1, 1, 1, 1);
    EXPECT_EQ(1, te.age());
    EXPECT_EQ(1, te.type());
    EXPECT_EQ(16777215, te.move());
    EXPECT_EQ(1, te.depth());
    EXPECT_EQ(-INFINITY_SCORE, te.score());

    // move range
    for (auto i = 0; i <= 16777215; ++i)
    {
        te.store(i, INFINITY_SCORE, 1, 1, 1, 1);
        EXPECT_EQ(i, te.move());
        EXPECT_EQ(INFINITY_SCORE, te.score());
        EXPECT_EQ(1, te.depth());
        EXPECT_EQ(1, te.age());
        EXPECT_EQ(1, te.type());
    }

    // depth range
    for (auto i = 0; i < 255; ++i)
    {
        te.store(16777215, -INFINITY_SCORE, i, 1, 1, 1);
        EXPECT_EQ(i, te.depth());
        EXPECT_EQ(1, te.age());
        EXPECT_EQ(1, te.type());
        EXPECT_EQ(16777215, te.move());
        EXPECT_EQ(-INFINITY_SCORE, te.score());
    }

    // score range
    for (auto i = -INFINITY_SCORE; i <= INFINITY_SCORE; ++i)
    {
        te.store(16777215, i, 1, 1, 1, 1);
        EXPECT_EQ(i, te.score());
        EXPECT_EQ(1, te.depth());
        EXPECT_EQ(1, te.age());
        EXPECT_EQ(1, te.type());
        EXPECT_EQ(16777215, te.move());
    }

    // type range
    for (auto i = 0; i < 3; ++i)
    {
        te.store(16777215, 1, 1, i, 1, 1);
        EXPECT_EQ(i, te.type());
        EXPECT_EQ(1, te.score());
        EXPECT_EQ(1, te.depth());
        EXPECT_EQ(1, te.age());
        EXPECT_EQ(16777215, te.move());
    }

    // age range
    for (auto i = 0; i < 255; ++i)
    {
        te.store(16777215, 1, 1, 1, 1, i);
        EXPECT_EQ(i, te.age());
        EXPECT_EQ(1, te.type());
        EXPECT_EQ(1, te.score());
        EXPECT_EQ(1, te.depth());
        EXPECT_EQ(16777215, te.move());
    }
}

}
