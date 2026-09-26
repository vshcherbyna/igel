/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2026 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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
#include "../nnue.h"
#include "../notation.h"
#include "../position.h"

#include <gtest/gtest.h>

#include <cstring>
#include <memory>
#include <vector>

namespace unit
{

#if !defined(PURE_HCE)

//
// The search leaves most positions without an accumulator of their own, so the ones it
// evaluates are brought up to date across several moves, forwards from an older accumulator
// or backwards from a refresh. Every route has to end at the accumulator a refresh builds.
//

static void checkAccumulator(Position & pos, Position & scratch, Evaluator & ev)
{
    ev.evaluate(pos);

    ASSERT_EQ(true, scratch.SetFEN(pos.FEN()));
    ev.evaluate(scratch);

    const Accumulator & reached = pos.state()->accumulator;
    const Accumulator & fresh   = scratch.state()->accumulator;

    EXPECT_EQ(0, std::memcmp(reached.accumulation, fresh.accumulation, sizeof(fresh.accumulation))) << "fen " << pos.FEN();
    EXPECT_EQ(0, std::memcmp(reached.psqtAccumulation, fresh.psqtAccumulation, sizeof(fresh.psqtAccumulation))) << "fen " << pos.FEN();
}

static void checkTree(Position & pos, Position & scratch, Evaluator & ev, int depth, bool evaluate)
{
    if (evaluate && (pos.Hash() >> 7) % 3)
        checkAccumulator(pos, scratch, ev);

    if (depth == 0)
        return;

    MoveList mvlist;

    if (pos.InCheck())
        GenMovesInCheck(pos, mvlist);
    else
        GenAllMoves(pos, mvlist);

    for (size_t i = 0; i < mvlist.Size(); ++i) {

        if (!pos.MakeMove(mvlist[i].m_mv))
            continue;

        checkTree(pos, scratch, ev, depth - 1, true);
        pos.UnmakeMove();
    }

    if (evaluate && depth > 1 && !pos.InCheck()) {
        pos.MakeNullMove();
        checkTree(pos, scratch, ev, depth - 1, false);
        pos.UnmakeNullMove();
    }
}

TEST(AccumulatorCatchUpTree, Positive)
{
    InitBitboards();
    Position::InitHashNumbers();

    if (!Evaluator::initEval())
        GTEST_SKIP() << "no usable embedded network; this check needs a real evaluation";

    struct Case { const char * fen; int depth; bool frc; };

    const Case cases[] = {
        { STD_POSITION,                                                          3, false },
        { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -",     2, false },
        { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -",                               4, false },
        { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",    2, false },
        { "8/PPPk4/8/8/8/8/4Kppp/8 w - - 0 1",                                   3, false },
        { "bqnb1rkr/pp3ppp/3ppn2/2p5/5P2/P2P4/NPP1P1PP/BQ1BNRKR w HFhf - 2 9",   2, true  },
    };

    const bool chess960 = g_uci_chess960;

    Evaluator ev;
    std::unique_ptr<Position> pos(new Position), scratch(new Position);

    for (const Case & c : cases) {

        g_uci_chess960 = c.frc;

        ASSERT_EQ(true, pos->SetFEN(c.fen));
        checkTree(*pos.get(), *scratch.get(), ev, c.depth, true);
    }

    g_uci_chess960 = chess960;
}

//
// The first line leaves more unevaluated positions behind than the catch-up walks back
// over, the second puts king moves among them. Stepping back to the start afterwards
// meets positions rebuilt on the way first, then ones never reached, and last the root,
// which shares its slot with the first move.
//

TEST(AccumulatorCatchUpLine, Positive)
{
    InitBitboards();
    Position::InitHashNumbers();

    if (!Evaluator::initEval())
        GTEST_SKIP() << "no usable embedded network; this check needs a real evaluation";

    const std::vector<std::vector<const char *>> lines = {
        { "g1f3", "g8f6", "f3g1", "f6g8", "b1c3", "b8c6", "c3b1", "c6b8", "g1f3", "g8f6", "f3g1", "f6g8" },
        { "e2e4", "e7e5", "e1e2", "e8e7", "g1f3", "g8f6", "e2d3", "b8c6", "b1c3", "e7d6", "d3c4", "f8e7" }
    };

    Evaluator ev;
    std::unique_ptr<Position> pos(new Position), scratch(new Position);

    for (const auto & line : lines) {

        ASSERT_EQ(true, pos->SetFEN(STD_POSITION));

        for (const char * mv : line)
            ASSERT_EQ(true, pos->MakeMove(StrToMove(mv, *pos.get()))) << mv;

        checkAccumulator(*pos.get(), *scratch.get(), ev);

        for (size_t i = 0; i < line.size(); ++i) {
            pos->UnmakeMove();
            checkAccumulator(*pos.get(), *scratch.get(), ev);
        }
    }
}

#endif

}
