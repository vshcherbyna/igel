/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2018-2026 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

#include "../bitboards.h"
#include "../moves.h"
#include "../search.h"
#include "../time.h"
#include "../nnue.h"
#include "../tt.h"
#include <gtest/gtest.h>
#include <memory>

namespace unit
{

namespace
{

//
//  Below MEDIUM_LEVEL abSearch does not hand a depth zero node to qsearch, it keeps
//  searching with the depth going negative, several dozen plies below zero on a real
//  search. Everything the node then indexes by depth has to survive that. Running the
//  search under a sanitizer is what gives these cases their teeth
//

bool evaluationReady()
{
#if !defined(PURE_HCE)
    return Evaluator::initEval();
#else
    return true;
#endif
}

uint64_t searchAtLevel(int level, const char * fen, const char * nodes) {
    InitBitboards();
    Position::InitHashNumbers();
    EXPECT_TRUE(TTable::instance().setHashSize(16, 1));

    auto search = std::make_unique<Search>();
    search->setLevel(level);
    EXPECT_TRUE(search->setFEN(fen));

    Time t;
    EXPECT_TRUE(t.parseTime({ "go", "nodes", nodes }, true));

    return search->startSearch(t, 1, false, true);
}

}

TEST(ReducedSkill, SearchesBelowDepthZeroStayInBounds) {
    InitBitboards();
    Position::InitHashNumbers();
    if (!evaluationReady())
        GTEST_SKIP() << "no usable embedded network; these checks need a real evaluation";

    //  level 0 allots no depth at all and so searches nothing; every level above it
    //  reaches the negative depth region and has to come back with a search behind it
    EXPECT_EQ(searchAtLevel(MIN_LEVEL, STD_POSITION, "30000"), 0u);

    for (int level = MIN_LEVEL + 1; level <= MEDIUM_LEVEL; ++level)
        EXPECT_GT(searchAtLevel(level, STD_POSITION, "30000"), 0u) << "level " << level;
}

TEST(ReducedSkill, BoundaryLevelsAroundTheQsearchSwitch) {
    InitBitboards();
    Position::InitHashNumbers();
    if (!evaluationReady())
        GTEST_SKIP() << "no usable embedded network; these checks need a real evaluation";

    //  MEDIUM_LEVEL keeps searching past depth zero, one above it drops into qsearch
    EXPECT_GT(searchAtLevel(MEDIUM_LEVEL, STD_POSITION, "40000"), 0u);
    EXPECT_GT(searchAtLevel(MEDIUM_LEVEL + 1, STD_POSITION, "40000"), 0u);
}

TEST(ReducedSkill, HoldsUpOnBusyPositions) {
    InitBitboards();
    Position::InitHashNumbers();
    if (!evaluationReady())
        GTEST_SKIP() << "no usable embedded network; these checks need a real evaluation";

    const char * positions[] = {
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1",
        "rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    };

    for (const char * fen : positions)
        EXPECT_GT(searchAtLevel(MIN_LEVEL + 1, fen, "30000"), 0u) << fen;
}

}
