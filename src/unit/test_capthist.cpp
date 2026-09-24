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
#include "../history.h"
#include "../moveeval.h"
#include "../moves.h"
#include "../nnue.h"
#include "../notation.h"
#include "../search.h"
#include "../tt.h"
#include <gtest/gtest.h>
#include <memory>
#include <vector>

namespace unit
{

namespace
{

//
//  Two winning captures of undefended pawns: the knight takes on d5 and the bishop on g4,
//  so the static order puts the knight first by the difference of the attackers alone
//

const char * TWO_CAPTURES = "4k3/8/8/3p4/6p1/2N5/8/3BK3 w - - 0 1";

//
//  The queen takes a pawn defended by the c6 pawn and loses 900 in the exchange
//

const char * LOSING_CAPTURE = "4k3/8/2p5/3p4/8/8/8/3QK3 w - - 0 1";

bool evaluationReady()
{
#if !defined(PURE_HCE)
    return Evaluator::initEval();
#else
    return true;
#endif
}

std::unique_ptr<Search> searchAt(const char * fen)
{
    InitBitboards();
    Position::InitHashNumbers();

    auto search = std::make_unique<Search>();
    search->clearHistory();
    search->clearKillers();
    search->clearStacks();
    EXPECT_TRUE(search->setFEN(fen));

    return search;
}

Move moveOf(Search & search, const char * move)
{
    Move mv = StrToMove(move, search.m_position);
    EXPECT_TRUE(mv != 0) << "illegal move " << move;
    return mv;
}

int sortScoreOf(Search & search, Move mv)
{
    MoveList mvlist;
    GenAllMoves(search.m_position, mvlist);
    MoveEval::sortMoves(&search, mvlist, Move{}, 0);

    for (size_t i = 0; i < mvlist.Size(); ++i) {
        if (mvlist[i].m_mv == mv)
            return mvlist[i].m_score;
    }

    ADD_FAILURE() << "move " << MoveToStrLong(mv) << " was not generated";
    return 0;
}

}

//
//  A cutoff trains every capture the node tried: the one that cut moves up, the ones that
//  were searched before it and failed move down
//

TEST(CaptureHistory, PromotesTheCuttingCaptureAndDemotesTheOthers)
{
    auto search = searchAt(TWO_CAPTURES);
    const Move knight = moveOf(*search, "c3d5");
    const Move bishop = moveOf(*search, "d1g4");

    const int knightBefore = sortScoreOf(*search, knight);
    const int bishopBefore = sortScoreOf(*search, bishop);
    ASSERT_GT(knightBefore, bishopBefore);

    const Move tried[] = { knight, bishop };
    History::updateCaptureHistory(search.get(), tried, 2, bishop, 4 * 4);

    EXPECT_LT(sortScoreOf(*search, knight), knightBefore);
    EXPECT_GT(sortScoreOf(*search, bishop), bishopBefore);
    EXPECT_GT(sortScoreOf(*search, bishop), sortScoreOf(*search, knight));
}

TEST(CaptureHistory, AQuietCutoffDemotesEveryCaptureTried)
{
    auto search = searchAt(TWO_CAPTURES);
    const Move knight = moveOf(*search, "c3d5");
    const Move bishop = moveOf(*search, "d1g4");

    const int knightBefore = sortScoreOf(*search, knight);
    const int bishopBefore = sortScoreOf(*search, bishop);

    const Move tried[] = { knight, bishop };
    History::updateCaptureHistory(search.get(), tried, 2, Move{}, 4 * 4);

    EXPECT_LT(sortScoreOf(*search, knight), knightBefore);
    EXPECT_LT(sortScoreOf(*search, bishop), bishopBefore);
}

//
//  The search keeps only the first 32 noisy moves it tries, so a cutoff by a later one is not
//  in the list and has to be rewarded anyway. Eight pawns on the seventh rank give 44 legal
//  promotions and captures, and the one that cuts shares its entry with a promotion that failed
//

TEST(CaptureHistory, RewardsACutoffBeyondTheTriedMovesLimit)
{
    auto search = searchAt("n1n1n1n1/PPPPPPPP/7k/8/8/8/8/K7 w - - 0 1");

    MoveList mvlist;
    GenAllMoves(search->m_position, mvlist);
    MoveEval::sortMoves(search.get(), mvlist, Move{}, 0);

    std::vector<Move> noisy;
    for (size_t i = 0; i < mvlist.Size(); ++i) {
        const Move mv = MoveEval::getNextBest(mvlist, i);
        if (MoveEval::isTacticalMove(mv) && search->m_position.MakeMove(mv)) {
            search->m_position.UnmakeMove();
            noisy.push_back(mv);
        }
    }
    ASSERT_GT(noisy.size(), 32u);

    const Move cutoff = noisy[32];
    const int cutoffBefore = sortScoreOf(*search, cutoff);
    const int firstBefore  = sortScoreOf(*search, noisy[0]);

    History::updateCaptureHistory(search.get(), noisy.data(), 32, cutoff, 20 * 20);

    EXPECT_GT(sortScoreOf(*search, cutoff), cutoffBefore);
    EXPECT_LT(sortScoreOf(*search, noisy[0]), firstBefore);
}

//
//  Losing captures are sorted by their exact exchange value, which the search reads back
//  instead of running SEE again, so history must never reach them
//

TEST(CaptureHistory, LeavesTheExchangeValueOfALosingCaptureIntact)
{
    auto search = searchAt(LOSING_CAPTURE);
    const Move queen = moveOf(*search, "d1d5");
    const int before = sortScoreOf(*search, queen);

    const Move tried[] = { queen };
    for (int i = 0; i < 100; ++i)
        History::updateCaptureHistory(search.get(), tried, 1, queen, 20 * 20);

    const int after = sortScoreOf(*search, queen);
    EXPECT_EQ(before, after);
    EXPECT_TRUE(MoveEval::cachedSeeNegative(after));
    EXPECT_EQ(-900, MoveEval::SEE(search.get(), queen));
    EXPECT_EQ(MoveEval::SEE(search.get(), queen), MoveEval::cachedSee(after));
}

//
//  However one-sided the evidence, an entry saturates instead of overflowing, and a winning
//  capture never falls out of the band the search relies on to tell it from a losing one
//

TEST(CaptureHistory, SaturatesInsideTheWinningCaptureBand)
{
    auto search = searchAt(TWO_CAPTURES);
    const Move knight = moveOf(*search, "c3d5");
    const Move tried[] = { knight };

    for (int i = 0; i < 100; ++i)
        History::updateCaptureHistory(search.get(), tried, 1, Move{}, 20 * 20);

    const int demoted = sortScoreOf(*search, knight);
    EXPECT_FALSE(MoveEval::cachedSeeNegative(demoted));

    History::updateCaptureHistory(search.get(), tried, 1, Move{}, 20 * 20);
    EXPECT_EQ(demoted, sortScoreOf(*search, knight));

    for (int i = 0; i < 200; ++i)
        History::updateCaptureHistory(search.get(), tried, 1, knight, 20 * 20);

    const int promoted = sortScoreOf(*search, knight);
    EXPECT_GT(promoted, demoted);

    History::updateCaptureHistory(search.get(), tried, 1, knight, 20 * 20);
    EXPECT_EQ(promoted, sortScoreOf(*search, knight));
}

TEST(CaptureHistory, ANewGameForgetsIt)
{
    auto search = searchAt(TWO_CAPTURES);
    const Move knight = moveOf(*search, "c3d5");
    const int before = sortScoreOf(*search, knight);

    const Move tried[] = { knight };
    History::updateCaptureHistory(search.get(), tried, 1, knight, 10 * 10);
    ASSERT_NE(before, sortScoreOf(*search, knight));

    search->clearHistory();
    EXPECT_EQ(before, sortScoreOf(*search, knight));
}

//
//  The search has to feed the table itself: after a short search of a busy middlegame, the
//  order of the captures on the board is no longer the static one
//

TEST(CaptureHistory, IsTrainedBySearch)
{
    auto search = searchAt("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    if (!evaluationReady())
        GTEST_SKIP() << "no usable embedded network; this check needs a real evaluation";
    ASSERT_TRUE(TTable::instance().setHashSize(16, 1));
    ASSERT_TRUE(TTable::instance().clearHash(1));

    MoveList captures;
    GenCapturesAndPromotions(search->m_position, captures);
    ASSERT_GT(captures.Size(), 0u);

    std::vector<int> before;
    for (size_t i = 0; i < captures.Size(); ++i)
        before.push_back(sortScoreOf(*search, captures[i].m_mv));

    Time time;
    ASSERT_TRUE(time.parseTime({ "go", "depth", "8" }, search->m_position.Side() == WHITE));
    EXPECT_GT(search->startSearch(time, 1, false, true), 0u);

    size_t trained = 0;
    for (size_t i = 0; i < captures.Size(); ++i)
        trained += sortScoreOf(*search, captures[i].m_mv) != before[i];

    EXPECT_GT(trained, 0u);
}

}
