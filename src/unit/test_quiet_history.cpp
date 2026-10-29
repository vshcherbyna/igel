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

#include "../history.h"
#include "../search.h"
#include "../notation.h"
#include "../moveeval.h"
#include <gtest/gtest.h>
#include <memory>
#include <set>

struct HistoryTestAccess {
    static int pawnIndex(const Search & s) {
        return Search::pawnHistoryIndex(s.m_position.pawnHash());
    }
    static int16_t & pawn(Search & s, Move m) {
        return s.m_pawnHistory->entry[pawnIndex(s)][m.Piece()][m.To()];
    }
    static int16_t & quiet(Search & s, Move m) {
        return s.m_history[GetColor(m.Piece())][m.From()][m.To()][History::threatIndex(m, History::enemyAttacks(s.m_position))];
    }
    static void seedStatic(Search & s, Move m, EVAL before, EVAL after) {
        s.m_moveStack[0] = m;
        s.m_quietHistoryStack[0] = m ? &quiet(s, m) : nullptr;
        s.m_rawEvalStack[0] = before;
        s.m_rawEvalStack[1] = after;
    }
    static Search & worker(Search & s) { return s.m_threadParams[0]; }
    static bool pointerCleared(Search & s) { return !s.m_quietHistoryStack[0]; }
};

namespace {
History::LegalQuietMask allLegalQuiets() {
    History::LegalQuietMask mask;
    mask.set();
    return mask;
}
std::unique_ptr<Search> historySearch(const char * fen = STD_POSITION) {
    InitBitboards();
    Position::InitHashNumbers();
    auto s = std::make_unique<Search>();
    s->clearHistory();
    s->clearStacks();
    s->clearKillers();
    EXPECT_TRUE(s->setFEN(fen));
    return s;
}
}

TEST(QuietHistory, StaticFeedbackUsesPreviousMoversPerspectiveAndRemovesTempo) {
    auto s = historySearch();
    const Move m(E2, E4, PW);
    HistoryTestAccess::seedStatic(*s, m, 20, -80);
    History::updateStaticHistory(s.get(), 1);
    EXPECT_GT(HistoryTestAccess::quiet(*s, m), 0);
    s->clearHistory();
    HistoryTestAccess::seedStatic(*s, m, 20, 120);
    History::updateStaticHistory(s.get(), 1);
    EXPECT_LT(HistoryTestAccess::quiet(*s, m), 0);
    s->clearHistory();
    HistoryTestAccess::seedStatic(*s, m, 70, -30);
    History::updateStaticHistory(s.get(), 1);
    EXPECT_EQ(HistoryTestAccess::quiet(*s, m), 0);
}

TEST(QuietHistory, StaticFeedbackRejectsNullTacticalAndInvalidEvaluation) {
    auto s = historySearch();
    const Move quiet(E2, E4, PW);
    for (const Move m : {Move{}, Move(E4, D5, PW, PB), Move(A7, A8, PW, 0, QW)}) {
        HistoryTestAccess::seedStatic(*s, m, 20, -300);
        History::updateStaticHistory(s.get(), 1);
        if (m) {
            EXPECT_EQ(HistoryTestAccess::quiet(*s, m), 0);
        }
    }
    for (EVAL bad : {NO_SCORE, CHECKMATE_SCORE, -CHECKMATE_SCORE, TBBASE_SCORE}) {
        HistoryTestAccess::seedStatic(*s, quiet, bad, -300);
        History::updateStaticHistory(s.get(), 1);
        EXPECT_EQ(HistoryTestAccess::quiet(*s, quiet), 0);
        HistoryTestAccess::seedStatic(*s, quiet, 20, bad);
        History::updateStaticHistory(s.get(), 1);
        EXPECT_EQ(HistoryTestAccess::quiet(*s, quiet), 0);
    }
    HistoryTestAccess::seedStatic(*s, quiet, 20, -300);
    History::updateStaticHistory(s.get(), 0);
    EXPECT_EQ(HistoryTestAccess::quiet(*s, quiet), 0);
}

TEST(QuietHistory, StaticFeedbackSaturatesAndCanReverse) {
    auto s = historySearch();
    const Move m(G1, F3, NW);
    HistoryTestAccess::seedStatic(*s, m, 20, -10000);
    for (int i = 0; i < 1000; ++i) History::updateStaticHistory(s.get(), 1);
    const int positive = HistoryTestAccess::quiet(*s, m);
    EXPECT_GT(positive, 0);
    EXPECT_LE(positive, 16384);
    History::updateStaticHistory(s.get(), 1);
    EXPECT_EQ(HistoryTestAccess::quiet(*s, m), positive);
    HistoryTestAccess::seedStatic(*s, m, 20, 10000);
    for (int i = 0; i < 1000; ++i) History::updateStaticHistory(s.get(), 1);
    EXPECT_LT(HistoryTestAccess::quiet(*s, m), 0);
    EXPECT_GE(HistoryTestAccess::quiet(*s, m), -16384);
}

TEST(QuietHistory, ResetClearsFeedbackAndPointersForEveryWorker) {
    auto s = historySearch();
    s->setThreadCount(1); // one helper in addition to the principal searcher
    s->clearHistory();
    s->clearStacks();
    const Move m(G1, F3, NW);
    for (Search * t : {s.get(), &HistoryTestAccess::worker(*s)}) {
        ASSERT_TRUE(t->setFEN(STD_POSITION));
        HistoryTestAccess::seedStatic(*t, m, 20, -80);
        History::updateStaticHistory(t, 1);
        ASSERT_GT(HistoryTestAccess::quiet(*t, m), 0);
    }
    s->clearHistory();
    for (Search * t : {s.get(), &HistoryTestAccess::worker(*s)}) {
        EXPECT_EQ(HistoryTestAccess::quiet(*t, m), 0);
        EXPECT_TRUE(HistoryTestAccess::pointerCleared(*t));
    }
}

TEST(QuietHistory, ThreatMapMatchesPositionAttackQueries) {
    for (const char * fen : {
        STD_POSITION,
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R b KQkq - 0 1",
        "4k3/8/8/3pP3/8/2n5/8/4K3 w - d6 0 1"}) {
        auto s = historySearch(fen);
        const U64 threats = History::enemyAttacks(s->m_position);
        for (FLD square = 0; square < 64; ++square)
            EXPECT_EQ(bool(threats & BB_SINGLE[square]),
                      s->m_position.IsAttacked(square, s->m_position.Side() ^ 1));
    }
}

TEST(QuietHistory, ThreatBucketsAreIndependentAndUsedByOrdering) {
    auto s = historySearch();
    const Move m(G1, F3, NW);
    const U64 masks[] = {0, BB_SINGLE[F3], BB_SINGLE[G1], BB_SINGLE[G1] | BB_SINGLE[F3]};
    for (int trained = 0; trained < 4; ++trained) {
        s->clearHistory();
        MoveList moves;
        moves.Add(m);
        History::updateHistory(s.get(), moves, 2, 16, masks[trained], allLegalQuiets());
        for (int queried = 0; queried < 4; ++queried) {
            History::HistoryHeuristics hh;
            History::fetchHistory(s.get(), m, 2, hh, masks[queried]);
            EXPECT_EQ(hh.history, trained == queried ? 512 : 0);
            MoveEval::sortMoves(s.get(), moves, Move{}, 2, masks[queried]);
            EXPECT_EQ(moves[0].m_score, hh.history + HistoryTestAccess::pawn(*s, m));
        }
    }
}

TEST(QuietHistory, StaticFeedbackKeepsParentThreatContextAfterMakeMove) {
    auto s = historySearch("4k3/8/8/8/7b/8/5P2/K7 w - - 0 1");
    const Move m(F2, F3, PW);
    const U64 parentThreats = History::enemyAttacks(s->m_position);
    HistoryTestAccess::seedStatic(*s, m, 20, -80);
    ASSERT_TRUE(s->m_position.MakeMove(m));
    // The pawn leaves the bishop diagonal; the side to move also changes.
    ASSERT_NE(History::threatIndex(m, parentThreats),
              History::threatIndex(m, History::enemyAttacks(s->m_position)));
    History::updateStaticHistory(s.get(), 1);
    s->m_position.UnmakeMove();
    History::HistoryHeuristics hh;
    History::fetchHistory(s.get(), m, 0, hh, parentThreats);
    EXPECT_GT(hh.history, 0);
}

TEST(QuietHistory, PawnHistorySharesStructureButSeparatesPieceColorAndDestination) {
    auto s = historySearch();
    const Move m(G1, F3, NW);
    MoveList moves;
    moves.Add(m);
    History::updateHistory(s.get(), moves, 2, 16, History::enemyAttacks(s->m_position), allLegalQuiets());
    EXPECT_EQ(HistoryTestAccess::pawn(*s, m), 512);
    EXPECT_EQ(HistoryTestAccess::pawn(*s, Move(G8, F3, NB)), 0);
    EXPECT_EQ(HistoryTestAccess::pawn(*s, Move(G1, H3, NW)), 0);
    EXPECT_EQ(HistoryTestAccess::pawn(*s, Move(G1, F3, BW)), 0);
    const int bucket = HistoryTestAccess::pawnIndex(*s);
    ASSERT_TRUE(s->setFEN("rnbqkbnr/pppppppp/8/8/8/2N5/PPPPPPPP/R1BQKBNR w KQkq - 0 1"));
    ASSERT_EQ(HistoryTestAccess::pawnIndex(*s), bucket);
    EXPECT_EQ(HistoryTestAccess::pawn(*s, m), 512);
    ASSERT_TRUE(s->setFEN("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1"));
    ASSERT_NE(HistoryTestAccess::pawnIndex(*s), bucket);
    EXPECT_EQ(HistoryTestAccess::pawn(*s, m), 0);
}

TEST(QuietHistory, PawnHistoryRewardsCutoffAndPenalizesEarlierQuiets) {
    auto s = historySearch();
    const Move earlier(B1, C3, NW), best(G1, F3, NW);
    MoveList moves;
    moves.Add(earlier);
    moves.Add(best);
    const U64 threats = History::enemyAttacks(s->m_position);
    for (int i = 0; i < 1000; ++i)
        History::updateHistory(s.get(), moves, 2, 1000, threats, allLegalQuiets());
    EXPECT_LT(HistoryTestAccess::pawn(*s, earlier), 0);
    EXPECT_GE(HistoryTestAccess::pawn(*s, earlier), -16384);
    EXPECT_GT(HistoryTestAccess::pawn(*s, best), 0);
    EXPECT_LE(HistoryTestAccess::pawn(*s, best), 16384);
}

TEST(QuietHistory, PawnHistoryIsPrivateAndResetForEveryWorker) {
    auto s = historySearch();
    s->setThreadCount(1);
    Search & worker = HistoryTestAccess::worker(*s);
    ASSERT_TRUE(worker.setFEN(STD_POSITION));
    const Move m(G1, F3, NW);
    HistoryTestAccess::pawn(*s, m) = 123;
    EXPECT_EQ(HistoryTestAccess::pawn(worker, m), 0);
    HistoryTestAccess::pawn(worker, m) = -456;
    EXPECT_EQ(HistoryTestAccess::pawn(*s, m), 123);
    s->clearHistory();
    EXPECT_EQ(HistoryTestAccess::pawn(*s, m), 0);
    EXPECT_EQ(HistoryTestAccess::pawn(worker, m), 0);
}

TEST(QuietHistory, PawnHistoryChangesOrderingButNotSearchHistory) {
    auto s = historySearch();
    const Move m(G1, F3, NW);
    const U64 threats = History::enemyAttacks(s->m_position);
    for (int value : {-4096, 0, 4096}) {
        HistoryTestAccess::pawn(*s, m) = value;
        History::HistoryHeuristics hh;
        History::fetchHistory(s.get(), m, 0, hh, threats);
        EXPECT_EQ(hh.history + hh.cmhistory + hh.fmhistory, 0);
        MoveList moves;
        moves.Add(m);
        MoveEval::sortMoves(s.get(), moves, Move{}, 0, threats);
        EXPECT_EQ(moves[0].m_score, value);
    }
}

TEST(QuietHistory, PawnHistoryIgnoresIllegalQuietsButKeepsLegalTraining) {
    auto s = historySearch("4r2k/8/8/8/8/8/4N3/4K3 w - - 0 1");
    const Move illegal(E2, F4, NW), earlier(E1, D2, KW), best(E1, F1, KW);
    ASSERT_FALSE(s->m_position.MakeMove(illegal)); // the knight is pinned
    ASSERT_TRUE(s->m_position.MakeMove(earlier));
    s->m_position.UnmakeMove();
    ASSERT_TRUE(s->m_position.MakeMove(best));
    s->m_position.UnmakeMove();

    MoveList moves;
    moves.Add(illegal);
    moves.Add(earlier);
    moves.Add(best);
    History::LegalQuietMask legalQuiets;
    legalQuiets.set(1);
    legalQuiets.set(2);
    const U64 threats = History::enemyAttacks(s->m_position);
    History::updateHistory(s.get(), moves, 2, 16, threats, legalQuiets);

    EXPECT_EQ(HistoryTestAccess::pawn(*s, illegal), 0);
    EXPECT_EQ(HistoryTestAccess::pawn(*s, earlier), -128); // a quarter of the cutoff's bonus
    EXPECT_EQ(HistoryTestAccess::pawn(*s, best), 512);
    // The existing main history is intentionally unaffected by this experiment.
    EXPECT_EQ(HistoryTestAccess::quiet(*s, illegal), -512);
    EXPECT_EQ(HistoryTestAccess::quiet(*s, earlier), -512);
}

TEST(QuietHistory, PawnHistoryTellsApartEveryFirstPawnMove) {
    auto s = historySearch();
    MoveList moves;
    GenAllMoves(s->m_position, moves);
    std::set<int> buckets;
    int pawnMoves = 0;
    for (size_t i = 0; i < moves.Size(); ++i) {
        const Move mv = moves[i].m_mv;
        if (GetPieceType(mv.Piece()) != PAWN)
            continue;
        ASSERT_TRUE(s->m_position.MakeMove(mv));
        buckets.insert(HistoryTestAccess::pawnIndex(*s));
        s->m_position.UnmakeMove();
        ++pawnMoves;
    }
    EXPECT_EQ(pawnMoves, 16);
    // The low key bits alone put 1.a4, 1.c4 and 1.d4 into one bucket
    EXPECT_EQ(buckets.size(), 16u);
}
