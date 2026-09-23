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
#include "../nnue.h"
#include "../search.h"
#include "../tt.h"
#include <gtest/gtest.h>
#include <memory>

namespace unit
{

namespace
{

bool evaluationReady()
{
#if !defined(PURE_HCE)
    return Evaluator::initEval();
#else
    return true;
#endif
}

}

TEST(AdaptiveSearch, RestoresPositionAndProducesLegalRootMove)
{
    InitBitboards();
    Position::InitHashNumbers();
    if (!evaluationReady())
        GTEST_SKIP() << "no usable embedded network; these checks need a real evaluation";
    ASSERT_TRUE(TTable::instance().setHashSize(16, 1));

    // Exercise quiets, captures, castling, promotions, en passant and check
    // evasions, both with a cold TT and with information from a previous search.
    const char * positions[] = {
        STD_POSITION,
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1",
        "rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3",
        "4k3/8/8/8/8/8/4r3/3QK3 w - - 0 1",
    };

    for (const char * fen : positions) {
        SCOPED_TRACE(fen);
        auto search = std::make_unique<Search>();
        ASSERT_TRUE(search->setFEN(fen));
        ASSERT_TRUE(TTable::instance().clearHash(1));
        const auto before = search->m_position.FEN();
        const auto hash = search->m_position.Hash();

        for (int pass = 0; pass < 2; ++pass) {
            Time time;
            ASSERT_TRUE(time.parseTime({ "go", "depth", "8" }, search->m_position.Side() == WHITE));
            EXPECT_GT(search->startSearch(time, 1, false, true), 0u);
            EXPECT_EQ(before, search->m_position.FEN());
            EXPECT_EQ(hash, search->m_position.Hash());

            TEntry entry{};
            ASSERT_TRUE(TTable::instance().retrieve(hash, entry));
            ASSERT_TRUE(isValidScore(entry.score));
            ASSERT_NE(0u, static_cast<U32>(entry.move));
            MoveList moves;
            GenAllMoves(search->m_position, moves);
            bool legal = false;
            for (size_t i = 0; i < moves.Size(); ++i) {
                if (moves[i].m_mv == entry.move && search->m_position.MakeMove(entry.move)) {
                    search->m_position.UnmakeMove();
                    legal = true;
                    break;
                }
            }
            EXPECT_TRUE(legal);
        }

        // Stop during search, then reuse the same searcher. Interrupted reduced
        // searches/re-searches must unwind every move and leave usable state.
        Time time;
        ASSERT_TRUE(time.parseTime({ "go", "nodes", "2000" }, search->m_position.Side() == WHITE));
        search->startSearch(time, 1, false, true);
        EXPECT_EQ(before, search->m_position.FEN());
        EXPECT_EQ(hash, search->m_position.Hash());
        ASSERT_TRUE(time.parseTime({ "go", "depth", "5" }, search->m_position.Side() == WHITE));
        EXPECT_GT(search->startSearch(time, 1, false, true), 0u);
        EXPECT_EQ(before, search->m_position.FEN());
    }
}

TEST(AdaptiveSearch, PreservesMateScore)
{
    InitBitboards();
    Position::InitHashNumbers();
    if (!evaluationReady())
        GTEST_SKIP() << "no usable embedded network; this check needs a real evaluation";
    ASSERT_TRUE(TTable::instance().setHashSize(16, 1));
    ASSERT_TRUE(TTable::instance().clearHash(1));
    auto search = std::make_unique<Search>();
    ASSERT_TRUE(search->setFEN("7k/8/5KQ1/8/8/8/8/8 w - - 0 1"));
    Time time;
    ASSERT_TRUE(time.parseTime({ "go", "depth", "8" }, true));
    search->startSearch(time, 1, false, true);
    TEntry entry{};
    ASSERT_TRUE(TTable::instance().retrieve(search->m_position.Hash(), entry));
    EXPECT_EQ(CHECKMATE_SCORE - 1, entry.score);
}

TEST(AdaptiveSearch, EndgameSearchStaysWithinNodeBudget)
{
    InitBitboards();
    Position::InitHashNumbers();
#if defined(PURE_HCE)
    GTEST_SKIP() << "the node budget is calibrated for the NNUE evaluation";
#endif
    if (!evaluationReady())
        GTEST_SKIP() << "no usable embedded network; this check needs a real evaluation";
    ASSERT_TRUE(TTable::instance().setHashSize(16, 1));

    // Blocked endings from the bench. If a quiet hash move is extended on history
    // alone, the depth stops shrinking along chains of hash moves and the search runs
    // out to MAX_PLY: depth 16 then costs 1.6M to 33M nodes instead of 60K to 180K.
    // The budget leaves room for tuning and only catches that kind of runaway.
    const char * positions[] = {
        "8/8/1p2k1p1/3p3p/1p1P1P1P/1P2PK2/8/8 w - - 3 54",
        "8/8/8/8/5kp1/P7/8/1K1N4 w - - 0 1",
    };

    for (const char * fen : positions) {
        SCOPED_TRACE(fen);

        // Start from the state a new game gets, so node counts are reproducible.
        ASSERT_TRUE(TTable::instance().clearHash(1));
        TTable::instance().clearAge();
        auto search = std::make_unique<Search>();
        search->clearHistory();
        search->clearKillers();
        search->clearStacks();
        ASSERT_TRUE(search->setFEN(fen));

        Time time;
        ASSERT_TRUE(time.parseTime({ "go", "depth", "16" }, search->m_position.Side() == WHITE));
        EXPECT_LT(search->startSearch(time, 1, false, true), 1000000u);
    }
}

}
