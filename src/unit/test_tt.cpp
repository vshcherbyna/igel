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
#include "../evaluate.h"
#include <gtest/gtest.h>

namespace unit
{

TEST(TranspositionTableSizeTest, Negative)
{
    EXPECT_EQ(false, TTable::instance().setHashSize(0, 1));
    EXPECT_EQ(false, TTable::instance().clearHash(1));
}

TEST(TranspositionTableSizeTest, Positive)
{
    for (auto i = 1; i <= 128; ++i)
        EXPECT_EQ(true, TTable::instance().setHashSize(i, 1));

    EXPECT_EQ(true, TTable::instance().clearHash(1));
}

TEST(TranspositionTableEntryTest, Positive)
{
    EXPECT_EQ(true, TTable::instance().setHashSize(2, 1));
    EXPECT_EQ(true, TTable::instance().clearHash(1));

    for (auto j = 1; j < 16384; ++j)
        TTable::instance().record(j, j, 3, 0, 1, j);

    for (auto j = 1; j < 16384; ++j) {
        TEntry hentry{};
        EXPECT_EQ(true, TTable::instance().retrieve(j, hentry));

        EXPECT_EQ(j, hentry.m_key ^ hentry.m_data.raw);
        EXPECT_EQ(j, hentry.m_data.move);
        EXPECT_EQ(j, hentry.m_data.score);
        EXPECT_EQ(3, hentry.m_data.depth);
        EXPECT_EQ(1, hentry.m_data.type);
    }
}

TEST(TranspositionTableEntryScoreTest, Positive)
{
    TEntry te{};

    EXPECT_EQ(0, te.m_data.raw);
    EXPECT_EQ(0, te.m_key);

    te.store(0, 0, 0, 0, 0, 0);

    EXPECT_EQ(0, te.m_data.score);

    for (auto i = -CHECKMATE_SCORE + 128; i <= CHECKMATE_SCORE + 128; ++i) {
        te.store(0, i, 0, 0, 0, 0);
        EXPECT_EQ(i, te.m_data.score);
    }
}

TEST(TranspositionTableEntryDepthTest, Positive)
{
    TEntry te{};

    EXPECT_EQ(0, te.m_data.raw);
    EXPECT_EQ(0, te.m_key);

    te.store(0, 0, 0, 0, 0, 0);

    EXPECT_EQ(0, te.m_data.depth);

    for (auto i = -128; i <= 127; ++i) {
        te.store(0, 0, i, 0, 0, 0);
        EXPECT_EQ(i, te.m_data.depth);
    }
}

TEST(TranspositionTableEntryMoveTest, Positive)
{
    TEntry te{};

    EXPECT_EQ(0, te.m_data.raw);
    EXPECT_EQ(0, te.m_key);

    te.store(0, 0, 0, 0, 0, 0);

    EXPECT_EQ(0, te.m_data.move);

    InitBitboards();
    Position::InitHashNumbers();
    Evaluator::initEval();

    MoveList mvlist;
    std::unique_ptr<Position> pos(new Position);
    pos->SetInitial();

    GenAllMoves(*pos, mvlist);
    auto mvSize = mvlist.Size();

    for (size_t i = 0; i < mvSize; ++i) {
        Move mv = mvlist[i].m_mv;
        te.store(mv, 0, 0, 0, 0, 0);
        EXPECT_EQ(mv, te.m_data.move);
    }
}

TEST(TranspositionTableEntryTypeTest, Positive)
{
    TEntry te{};

    EXPECT_EQ(0, te.m_data.raw);
    EXPECT_EQ(0, te.m_key);

    te.store(0, 0, 0, 0, 0, 0);
    EXPECT_EQ(0, te.m_data.type);

    for (auto i = 0; i < 3; ++i)
    {
        te.store(0, 0, 0, i, 0, 0);
        EXPECT_EQ(i, te.m_data.type);
    }
}

TEST(TranspositionTableEntryAgeTest, Positive)
{
    TEntry te{};

    EXPECT_EQ(0, te.m_data.raw);
    EXPECT_EQ(0, te.m_key);

    te.store(0, 0, 0, 0, 0, 0);
    EXPECT_EQ(0, te.m_data.age);

    for (auto i = 0; i < 255; ++i) {
        te.store(0, 0, 0, 0, 0, i);
        EXPECT_EQ(i, te.m_data.age);
    }
}

TEST(TranspositionTableEntryCompositeTest, Positive)
{
    TEntry te{};

    EXPECT_EQ(0, te.m_data.raw);
    EXPECT_EQ(0, te.m_key);

    te.store(0, 0, 0, 0, 0, 0);

    EXPECT_EQ(0, te.m_data.age);
    EXPECT_EQ(0, te.m_data.type);
    EXPECT_EQ(0, te.m_data.move);
    EXPECT_EQ(0, te.m_data.depth);
    EXPECT_EQ(0, te.m_data.score);

    te.store(1, 1, 1, 1, 1, 1);

    EXPECT_EQ(1, te.m_data.age);
    EXPECT_EQ(1, te.m_data.type);
    EXPECT_EQ(1, te.m_data.move);
    EXPECT_EQ(1, te.m_data.depth);
    EXPECT_EQ(1, te.m_data.score);

    te.store(16777215, CHECKMATE_SCORE, 1, 1, 1, 1);
    EXPECT_EQ(1, te.m_data.age);
    EXPECT_EQ(1, te.m_data.type);
    EXPECT_EQ(16777215, te.m_data.move);
    EXPECT_EQ(1, te.m_data.depth);
    EXPECT_EQ(CHECKMATE_SCORE, te.m_data.score);

    te.store(16777215, -CHECKMATE_SCORE, 1, 1, 1, 1);
    EXPECT_EQ(1, te.m_data.age);
    EXPECT_EQ(1, te.m_data.type);
    EXPECT_EQ(16777215, te.m_data.move);
    EXPECT_EQ(1, te.m_data.depth);
    EXPECT_EQ(-CHECKMATE_SCORE, te.m_data.score);

     // move range
    for (auto i = 0; i <= 16777215; ++i) {
        te.store(i, CHECKMATE_SCORE, 1, 1, 1, 1);
        EXPECT_EQ(i, te.m_data.move);
        EXPECT_EQ(CHECKMATE_SCORE, te.m_data.score);
        EXPECT_EQ(1, te.m_data.depth);
        EXPECT_EQ(1, te.m_data.age);
        EXPECT_EQ(1, te.m_data.type);
    }

     // depth range
    for (auto i = -128; i <= 127; ++i) {
        te.store(16777215, -CHECKMATE_SCORE, i, 1, 1, 1);
        EXPECT_EQ(i, te.m_data.depth);
        EXPECT_EQ(1, te.m_data.age);
        EXPECT_EQ(1, te.m_data.type);
        EXPECT_EQ(16777215, te.m_data.move);
        EXPECT_EQ(-CHECKMATE_SCORE, te.m_data.score);
    }

    // score range
    for (auto i = -CHECKMATE_SCORE + 128; i <= CHECKMATE_SCORE + 128; ++i) {
        te.store(16777215, i, 1, 1, 1, 1);
        EXPECT_EQ(i, te.m_data.score);
        EXPECT_EQ(1, te.m_data.depth);
        EXPECT_EQ(1, te.m_data.age);
        EXPECT_EQ(1, te.m_data.type);
        EXPECT_EQ(16777215, te.m_data.move);
    }

    // type range
    for (auto i = 0; i < 3; ++i) {
        te.store(16777215, 1, 1, i, 1, 1);
        EXPECT_EQ(i, te.m_data.type);
        EXPECT_EQ(1, te.m_data.score);
        EXPECT_EQ(1, te.m_data.depth);
        EXPECT_EQ(1, te.m_data.age);
        EXPECT_EQ(16777215, te.m_data.move);
    }

    // age range
    for (auto i = 0; i < 255; ++i) {
        te.store(16777215, 1, 1, 1, 1, i);
        EXPECT_EQ(i, te.m_data.age);
        EXPECT_EQ(1, te.m_data.type);
        EXPECT_EQ(1, te.m_data.score);
        EXPECT_EQ(1, te.m_data.depth);
        EXPECT_EQ(16777215, te.m_data.move);
    }
}

}
