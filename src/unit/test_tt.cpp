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

#include "../tt.h"
#include "../moves.h"
#include "../nnue.h"
#include "../search.h"
#if defined(PURE_HCE)
#include "../hce.h"
#endif
#include "../time.h"
#if defined(SYZYGY_SUPPORT)
#include "../fathom/tbprobe.h"
#endif
#include <vector>
#include <gtest/gtest.h>

namespace unit
{

//
//  Builds a hash whose cluster index and whose verification key both vary with n.
//  The index comes from the low bits and the key from the top 16, so a test that only
//  varies the low bits would not exercise key matching at all.
//

static U64 H(U64 n) { return (n << 48) | n; }

//
//  The table stores an entry packed into 12 bytes, five to a 64 byte cluster. depth is
//  biased by DEPTH_NONE so that an all-zero slot reads as unoccupied.
//

TEST(TranspositionTableLayoutTest, Positive)
{
    EXPECT_EQ(12u, sizeof(TTEntry));
    EXPECT_EQ(64u, sizeof(TTCluster));
    EXPECT_EQ(5, TT_CLUSTER_SIZE);

    TTEntry te{};
    EXPECT_EQ(false, te.isOccupied());

    te.store(H(0), 0, 0, 0, DEPTH_NONE + 1, HASH_ALPHA, false, 0);
    EXPECT_EQ(true, te.isOccupied());
    EXPECT_EQ(DEPTH_NONE + 1, te.depth());
}

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
        TTable::instance().record(j, j, -j, 3, 0, HASH_EXACT, (j & 1) != 0, H(j));

    for (auto j = 1; j < 16384; ++j) {
        TEntry hentry{};
        EXPECT_EQ(true, TTable::instance().retrieve(H(j), hentry));

        EXPECT_EQ(j, hentry.move);
        EXPECT_EQ(j, hentry.score);
        EXPECT_EQ(-j, hentry.eval);
        EXPECT_EQ(3, hentry.depth);
        EXPECT_EQ(HASH_EXACT, hentry.type);
        EXPECT_EQ((j & 1) != 0, hentry.pv);
    }
}

//
//  The cluster index is taken from the low bits of the hash, so a verification key built
//  from those same bits would be identical for every entry in a cluster and would match
//  any probe of it. Two positions sharing a cluster must stay distinguishable.
//

TEST(TranspositionTableKeyTest, Positive)
{
    const U64 a = 0x0000000000001234ull;
    const U64 b = 0xABCD000000001234ull;   // same cluster, different position

    EXPECT_NE(TTEntry::keyOf(a), TTEntry::keyOf(b));

    EXPECT_EQ(true, TTable::instance().setHashSize(2, 1));
    EXPECT_EQ(true, TTable::instance().clearHash(1));

    TTable::instance().record(111, 222, 333, 5, 0, HASH_EXACT, false, a);

    TEntry e{};
    EXPECT_EQ(true, TTable::instance().retrieve(a, e));
    EXPECT_EQ(222, e.score);
    EXPECT_EQ(333, e.eval);

    EXPECT_EQ(false, TTable::instance().retrieve(b, e));
}

//
//  A miss must not hand the search a stale snapshot
//

TEST(TranspositionTableRetrieveTest, Negative)
{
    EXPECT_EQ(true, TTable::instance().setHashSize(2, 1));
    EXPECT_EQ(true, TTable::instance().clearHash(1));

    TEntry hentry{};
    EXPECT_EQ(false, TTable::instance().retrieve(0x123456789abcdefull, hentry));

    EXPECT_EQ(NO_SCORE, hentry.score);
    EXPECT_EQ(NO_SCORE, hentry.eval);
    EXPECT_EQ(DEPTH_NONE, hentry.depth);
}

TEST(TranspositionTableEntryScoreTest, Positive)
{
    TTEntry te{};

    te.store(H(1), 1, 0, 0, 1, HASH_ALPHA, false, 0);
    EXPECT_EQ(0, te.read().score);

    //
    //  the widest score the search can hand to the table is a mate score adjusted by the ply
    //

    for (auto i = -CHECKMATE_SCORE - 128; i <= CHECKMATE_SCORE + 128; ++i) {
        te.store(H(1), 1, i, 0, 1, HASH_ALPHA, false, 0);
        EXPECT_EQ(i, te.read().score);
    }

    te.store(H(1), 1, NO_SCORE, 0, 1, HASH_NONE, false, 0);
    EXPECT_EQ(NO_SCORE, te.read().score);
}

TEST(TranspositionTableEntryEvalTest, Positive)
{
    TTEntry te{};

    for (auto i = -DECISIVE_SCORE + 1; i < DECISIVE_SCORE; ++i) {
        te.store(H(1), 1, 0, i, 1, HASH_ALPHA, false, 0);
        EXPECT_EQ(i, te.read().eval);
        EXPECT_EQ(true, isValidEval(te.read().eval));
    }

    //
    //  NO_SCORE means "no eval to offer", so a cached one for the same position survives
    //

    te.store(H(1), 1, 0, NO_SCORE, 1, HASH_ALPHA, false, 0);
    EXPECT_EQ(DECISIVE_SCORE - 1, te.read().eval);

    //
    //  a different position has nothing worth preserving
    //

    te.store(H(2), 1, 0, NO_SCORE, 1, HASH_ALPHA, false, 0);
    EXPECT_EQ(NO_SCORE, te.read().eval);
}

//
//  A write carrying no move or no eval must not erase what we already know about the
//  position: tablebase hits, stand-pat cutoffs and eval-only writes all arrive that way
//

TEST(TranspositionTableEntryPreservationTest, Positive)
{
    TTEntry te{};

    te.store(H(0xbeef), 4242, 100, 55, 7, HASH_EXACT, true, 3);
    EXPECT_EQ(4242, te.read().move);
    EXPECT_EQ(55, te.read().eval);

    // same position, no move and no eval to offer
    te.store(H(0xbeef), 0, 200, NO_SCORE, 9, HASH_BETA, false, 3);
    EXPECT_EQ(4242, te.read().move);
    EXPECT_EQ(55, te.read().eval);
    EXPECT_EQ(200, te.read().score);
    EXPECT_EQ(9, te.read().depth);

    // a different position takes the slot over completely
    te.store(H(0xf00d), 0, 300, NO_SCORE, 9, HASH_BETA, false, 3);
    EXPECT_EQ(0, te.read().move);
    EXPECT_EQ(NO_SCORE, te.read().eval);
}

//
//  Skill levels at or below MEDIUM_LEVEL keep running alpha-beta below depth zero, so the
//  table has to survive deeply negative depths. Anything it cannot encode is clamped, never
//  wrapped: a wrapped depth reads back as a very DEEP entry and would authorise cutoffs.
//

TEST(TranspositionTableEntryDepthClampTest, Positive)
{
    TTEntry te{};

    for (auto i : { -8, -7, -6, -127, -128, -200, -1000, 127, 128, 4096 }) {
        te.store(H(1), 1, 0, 0, i, HASH_ALPHA, false, 0);

        const int got = te.read().depth;
        EXPECT_EQ(true, te.isOccupied());
        EXPECT_GE(got, DEPTH_UNSEARCHED);
        EXPECT_LE(got, DEPTH_MAX);
        EXPECT_EQ(std::max(DEPTH_UNSEARCHED, std::min(i, DEPTH_MAX)), got);
    }
}

//
//  An eval-only entry carries NO_SCORE, and a racing read can pair a real bound with it.
//  A consumer that trusted the bound alone would return NO_SCORE as a winning cutoff.
//

TEST(TranspositionTableScoreValidityTest, Positive)
{
    EXPECT_EQ(false, isValidScore(NO_SCORE));
    EXPECT_EQ(false, isValidScore(-NO_SCORE));
    EXPECT_EQ(true,  isValidScore(CHECKMATE_SCORE + MAX_PLY));
    EXPECT_EQ(true,  isValidScore(-CHECKMATE_SCORE - MAX_PLY));
    EXPECT_EQ(true,  isValidScore(0));

    EXPECT_EQ(false, isValidEval(DECISIVE_SCORE));
    EXPECT_EQ(false, isValidEval(NO_SCORE));
    EXPECT_EQ(true,  isValidEval(DECISIVE_SCORE - 1));

    TTEntry te{};
    te.store(H(1), 0, NO_SCORE, 100, DEPTH_UNSEARCHED, HASH_NONE, false, 0);
    EXPECT_EQ(false, isValidScore(te.read().score));

    // the dangerous pairing: a real bound sitting on a score that is not one
    te.store(H(1), 0, NO_SCORE, 100, 8, HASH_BETA, false, 0);
    EXPECT_EQ(HASH_BETA, te.read().type);
    EXPECT_EQ(false, isValidScore(te.read().score));
}

//
//  Mate and tablebase scores are stored relative to the node and read back relative to the
//  root; everything else passes through untouched, NO_SCORE included
//

TEST(TranspositionTableScoreConversionTest, Positive)
{
    for (int ply = 0; ply < MAX_PLY; ++ply) {
        for (auto v : { 0, 1, -1, 500, -500, DECISIVE_SCORE - 1, -DECISIVE_SCORE + 1,
                        CHECKMATE_SCORE, -CHECKMATE_SCORE }) {
            EXPECT_EQ(v, scoreFromTT(scoreToTT(v, ply), ply));
            EXPECT_EQ(true, std::abs(scoreToTT(v, ply)) <= CHECKMATE_SCORE + MAX_PLY);
        }

        EXPECT_EQ(NO_SCORE, scoreToTT(NO_SCORE, ply));
        EXPECT_EQ(NO_SCORE, scoreFromTT(NO_SCORE, ply));
    }

    // a mate is nearer from the root than from a node deep in the tree
    EXPECT_EQ(CHECKMATE_SCORE - 10, scoreFromTT(scoreToTT(CHECKMATE_SCORE - 10, 10), 10));
    EXPECT_EQ(CHECKMATE_SCORE, scoreToTT(CHECKMATE_SCORE - 10, 10));
}

//
//  The tablebase producer makes a ROOT-relative score and record() is what normalises it.
//  A round trip of the helpers alone cannot catch a producer that passes the wrong ply, so
//  drive the real call the way search.cpp does.
//

TEST(TranspositionTableTablebaseScoreTest, Positive)
{
    EXPECT_EQ(true, TTable::instance().setHashSize(4, 1));

    EXPECT_EQ(true, TTable::instance().clearHash(1));

    for (int ply = 0; ply < MAX_PLY; ++ply) {
        for (int sign : { +1, -1 }) {
            //  exactly what the Syzygy probe in abSearch produces
            const EVAL produced = sign > 0 ?  TBBASE_SCORE - MAX_PLY - ply
                                           : -TBBASE_SCORE + MAX_PLY + ply;
            const U64  hash     = H(0x51D0 + ply * 8 + (sign > 0));
            const U8   type     = sign > 0 ? HASH_BETA : HASH_ALPHA;

            TTable::instance().record(0, produced, NO_SCORE, 12, ply, type, false, hash);

            TEntry e{};
            EXPECT_EQ(true, TTable::instance().retrieve(hash, e));

            //  read back at the ply it was stored at: must be what the probe produced
            EXPECT_EQ(produced, scoreFromTT(e.score, ply));

            //  and it must still read as a proven result
            EXPECT_EQ(true, std::abs(scoreFromTT(e.score, ply)) >= DECISIVE_SCORE);
        }
    }
}

//
//  An empty slot reads as key 0 with eval 0. A position whose signature is also 0, writing
//  no eval of its own, must not inherit that zero as if it were a real evaluation.
//

TEST(TranspositionTableEmptySlotTest, Positive)
{
    TTEntry te{};

    EXPECT_EQ(0u, TTEntry::keyOf(0x1234ull));   // a real hash with a zero signature
    EXPECT_EQ(false, te.isOccupied());

    te.store(0x1234ull, 0, 100, NO_SCORE, 5, HASH_BETA, false, 0);

    EXPECT_EQ(true, te.isOccupied());
    EXPECT_EQ(NO_SCORE, te.read().eval);              // not 0: nobody computed one
    EXPECT_EQ(false, isValidEval(te.read().eval));
    EXPECT_EQ(100, te.read().score);

    //  once a real eval is there, a later write with none preserves it
    te.store(0x1234ull, 0, 120, 77, 6, HASH_BETA, false, 0);
    te.store(0x1234ull, 0, 130, NO_SCORE, 7, HASH_BETA, false, 0);
    EXPECT_EQ(77, te.read().eval);
}

//
//  The cached eval is the UNDAMPED one, and Igel's position hash deliberately excludes the
//  halfmove clock, so ONE entry serves a board at every value of that clock. The property
//  that has to hold is that reusing a cached eval agrees with evaluating from scratch at
//  whatever clock it is reused at - which means comparing a real evaluation of a real
//  position against the cached path, not comparing a helper to itself.
//

TEST(TranspositionTableEvalFiftyTest, Positive)
{
    InitBitboards();
    Position::InitHashNumbers();
#if !defined(PURE_HCE)
    if (!Evaluator::initEval())
        GTEST_SKIP() << "no usable embedded network; this check needs a real evaluation";
#endif

    //  the same board at five different halfmove clocks
    static const char * const fens[] = {
        "r1bq1rk1/pp2bppp/2n1pn2/2pp4/3P4/2NBPN2/PPP2PPP/R1BQ1RK1 w - - 0 12",
        "r1bq1rk1/pp2bppp/2n1pn2/2pp4/3P4/2NBPN2/PPP2PPP/R1BQ1RK1 w - - 1 12",
        "r1bq1rk1/pp2bppp/2n1pn2/2pp4/3P4/2NBPN2/PPP2PPP/R1BQ1RK1 w - - 40 32",
        "r1bq1rk1/pp2bppp/2n1pn2/2pp4/3P4/2NBPN2/PPP2PPP/R1BQ1RK1 w - - 80 52",
        "r1bq1rk1/pp2bppp/2n1pn2/2pp4/3P4/2NBPN2/PPP2PPP/R1BQ1RK1 w - - 99 62",
    };
    static const int clocks[] = { 0, 1, 40, 80, 99 };
    const int N = 5;

    //  a Position carries a 2048 deep undo stack of Accumulators, ~8.6 MB - heap, not stack
    Evaluator ev;
    std::unique_ptr<Position> pos[5];
    EVAL raw[5], fresh[5];

    for (int i = 0; i < N; ++i) {
        pos[i].reset(new Position);
        ASSERT_EQ(true, pos[i]->SetFEN(fens[i]));
        raw[i]   = ev.evaluateRaw(*pos[i]);
        fresh[i] = ev.evaluate(*pos[i]);
        EXPECT_EQ(clocks[i], pos[i]->Fifty());

        //  one entry really does serve them all: the clock is not in the key
        EXPECT_EQ(pos[0]->Hash(), pos[i]->Hash());

        //  ...so the raw value cached under that one key is the same for all of them
        EXPECT_EQ(raw[0], raw[i]);
    }

    //  seed the table from the FIRST position only, then reuse it at the other two
    EXPECT_EQ(true, TTable::instance().setHashSize(4, 1));
    EXPECT_EQ(true, TTable::instance().clearHash(1));
    TTable::instance().record(0, NO_SCORE, raw[0], DEPTH_UNSEARCHED, 0, HASH_NONE, false, pos[0]->Hash());

    for (int i = 0; i < N; ++i) {
        TEntry e{};
        EXPECT_EQ(true, TTable::instance().retrieve(pos[i]->Hash(), e));
        EXPECT_EQ(true, isValidEval(e.eval));

        //  THE property: cached-then-redamped == evaluated from scratch, at this clock
        EXPECT_EQ(fresh[i], Evaluator::fromRaw(e.eval, pos[i]->Fifty()));
    }

#if !defined(PURE_HCE)
    //  the clock has to actually matter, or the check above proves nothing
    EXPECT_NE(fresh[0], fresh[N - 1]);
    EXPECT_GT(std::abs(fresh[0]), std::abs(fresh[N - 1]));

    //  Negative control: caching the DAMPED value is the bug this guards against. It has to
    //  be damped at a NON-ZERO clock to be a control at all - at clock 0 the damping factor
    //  is 1, so a value "damped" there is just the raw one and nothing would be detected.
    //  So: take what a buggy cache would have stored from the clock-80 position, reuse it at
    //  clock 0, and require the answer to be wrong.
    ASSERT_NE(0, raw[0]);   // a zero evaluation damps to itself and proves nothing
    const EVAL damagedByCachingDamped = fresh[N - 1] - Evaluator::Tempo;
    EXPECT_NE(fresh[0], Evaluator::fromRaw(damagedByCachingDamped, pos[0]->Fifty()));
#else
    //  the hand-crafted evaluation is not damped by the clock at all, by design
    EXPECT_EQ(fresh[0], fresh[N - 1]);
#endif
}

//
//  The search builds evaluations of its own - the null-move approximation negates the
//  parent's and adds two tempi - and those need the same bound as a computed one
//

TEST(TranspositionTableNullEvalBoundTest, Positive)
{
    const EVAL extreme = Evaluator::fromRaw(-DECISIVE_SCORE + 1, 0);

    EXPECT_EQ(true, isValidEval(extreme));
    EXPECT_EQ(true, isValidEval(Evaluator::bound(-extreme + 2 * Evaluator::Tempo)));
    EXPECT_EQ(true, isValidEval(Evaluator::bound(DECISIVE_SCORE + 1000)));
    EXPECT_EQ(true, isValidEval(Evaluator::bound(-DECISIVE_SCORE - 1000)));
}

//
//  Mate and tablebase scores are relative to the node when stored and to the root when read,
//  so a fixed window below CHECKMATE_SCORE classifies them differently depending on the ply
//  they are read at - a stored 31980 read at ply 50 becomes 31930 and falls outside a +/-50
//  window that excluded it before. isDecisiveScore() is ply-independent by construction, and
//  the singular guard depends on that: a proven result must never be a singular candidate.
//

TEST(TranspositionTableDecisiveClassificationTest, Positive)
{
    EXPECT_EQ(false, isDecisiveScore(0));
    EXPECT_EQ(false, isDecisiveScore(DECISIVE_SCORE - 1));
    EXPECT_EQ(true,  isDecisiveScore(DECISIVE_SCORE));
    EXPECT_EQ(true,  isDecisiveScore(-DECISIVE_SCORE));
    EXPECT_EQ(false, isDecisiveScore(NO_SCORE));    // not a score at all

    //  every mate distance, read back at every ply, must classify the same way
    for (int matePly = 0; matePly < MAX_PLY; ++matePly) {
        for (int sign : { +1, -1 }) {
            const EVAL atRoot = sign > 0 ? CHECKMATE_SCORE - matePly : -CHECKMATE_SCORE + matePly;

            for (int ply = 0; ply < MAX_PLY; ply += 7) {
                const EVAL stored  = scoreToTT(atRoot, ply);
                const EVAL decoded = scoreFromTT(stored, ply);

                EXPECT_EQ(atRoot, decoded);
                EXPECT_EQ(true, isDecisiveScore(decoded));
                EXPECT_EQ(true, isDecisiveScore(stored));
                EXPECT_EQ(true, isValidScore(stored));
            }
        }
    }

    //  ...and a long tablebase win is decisive at every ply too, which the old fixed
    //  mate window did not capture at all
    for (int ply = 0; ply < MAX_PLY; ply += 7) {
        const EVAL tb = TBBASE_SCORE - MAX_PLY - ply;
        EXPECT_EQ(true, isDecisiveScore(tb));
        EXPECT_EQ(true, isDecisiveScore(scoreToTT(tb, ply)));
        EXPECT_EQ(tb, scoreFromTT(scoreToTT(tb, ply), ply));
    }
}

//
//  An eval-only write carries nothing but a number the search can recompute, so it must not
//  buy its way into a full cluster by evicting a real result. It may still take an empty slot
//  or one whose generation has passed. Regression for the case where the guard sat inside the
//  matching-key branch and a cluster MISS evicted a live entry anyway.
//

TEST(TranspositionTableReplacementPolicyTest, Positive)
{
    //  a 2 MB table indexes on the low 15 bits, so these all land in one cluster
    //  while differing in the top-16 signature
    auto sameCluster = [](U64 sig) { return (sig << 48) | 0x1357ull; };

    EXPECT_EQ(true, TTable::instance().setHashSize(2, 1));
    EXPECT_EQ(true, TTable::instance().clearHash(1));
    TTable::instance().clearAge();

    //  fill every slot with a current-generation exact result
    for (int i = 0; i < TT_CLUSTER_SIZE; ++i)
        TTable::instance().record(1000 + i, 50 + i, 10 + i, 21 + i, 0, HASH_EXACT, false,
                                  sameCluster(0xA000 + i));

    for (int i = 0; i < TT_CLUSTER_SIZE; ++i) {
        TEntry e{};
        EXPECT_EQ(true, TTable::instance().retrieve(sameCluster(0xA000 + i), e));
        EXPECT_EQ(21 + i, e.depth);
    }

    //  an unrelated eval-only write must be declined: the cluster is full of live results
    TTable::instance().record(0, NO_SCORE, 777, DEPTH_UNSEARCHED, 0, HASH_NONE, false,
                              sameCluster(0xBEEF));

    TEntry probe{};
    EXPECT_EQ(false, TTable::instance().retrieve(sameCluster(0xBEEF), probe));

    for (int i = 0; i < TT_CLUSTER_SIZE; ++i) {
        TEntry e{};
        EXPECT_EQ(true, TTable::instance().retrieve(sameCluster(0xA000 + i), e));
        EXPECT_EQ(21 + i, e.depth);          // every real result survived
        EXPECT_EQ(HASH_EXACT, e.type);
    }

    //  ...but once the generation has moved on, those results are fair game
    TTable::instance().increaseAge();
    TTable::instance().record(0, NO_SCORE, 777, DEPTH_UNSEARCHED, 0, HASH_NONE, false,
                              sameCluster(0xBEEF));

    EXPECT_EQ(true, TTable::instance().retrieve(sameCluster(0xBEEF), probe));
    EXPECT_EQ(777, probe.eval);
    EXPECT_EQ(HASH_NONE, probe.type);

    //  an eval-only slot is the cheapest thing in a cluster, so a later eval-only write
    //  recycles it rather than being declined or evicting a real result
    EXPECT_EQ(true, TTable::instance().clearHash(1));
    TTable::instance().clearAge();

    for (int i = 0; i < TT_CLUSTER_SIZE - 1; ++i)
        TTable::instance().record(1000 + i, 50 + i, 10 + i, 21 + i, 0, HASH_EXACT, false,
                                  sameCluster(0xC000 + i));
    TTable::instance().record(0, NO_SCORE, 111, DEPTH_UNSEARCHED, 0, HASH_NONE, false,
                              sameCluster(0xC0FF));            // the one expendable slot

    TTable::instance().record(0, NO_SCORE, 222, DEPTH_UNSEARCHED, 0, HASH_NONE, false,
                              sameCluster(0xDEAD));            // a different position

    TEntry recycled{};
    EXPECT_EQ(true, TTable::instance().retrieve(sameCluster(0xDEAD), recycled));
    EXPECT_EQ(222, recycled.eval);
    EXPECT_EQ(false, TTable::instance().retrieve(sameCluster(0xC0FF), recycled));

    for (int i = 0; i < TT_CLUSTER_SIZE - 1; ++i) {
        TEntry e{};
        EXPECT_EQ(true, TTable::instance().retrieve(sameCluster(0xC000 + i), e));
        EXPECT_EQ(21 + i, e.depth);          // no real result was touched
    }

    //  restore the full-of-live-results cluster for the checks below
    EXPECT_EQ(true, TTable::instance().clearHash(1));
    TTable::instance().clearAge();
    for (int i = 0; i < TT_CLUSTER_SIZE; ++i)
        TTable::instance().record(1000 + i, 50 + i, 10 + i, 21 + i, 0, HASH_EXACT, false,
                                  sameCluster(0xA000 + i));

    //  and a matching key is always refreshed in place, whatever the generation
    TTable::instance().record(0, NO_SCORE, 888, DEPTH_UNSEARCHED, 0, HASH_NONE, false,
                              sameCluster(0xA002));
    TEntry kept{};
    EXPECT_EQ(true, TTable::instance().retrieve(sameCluster(0xA002), kept));
    EXPECT_EQ(888, kept.eval);               // eval updated
    EXPECT_EQ(23, kept.depth);               // score, depth and bound untouched
    EXPECT_EQ(HASH_EXACT, kept.type);
    EXPECT_EQ(52, kept.score);
}

//
//  A transposition hit that carries no eval used to make qSearch recompute the network on
//  every visit and throw the result away. This drives a REAL search rather than calling
//  record() directly, because the defect was a call site keeping an older guard while the
//  equivalent abSearch path was fixed - which is precisely what a helper-level test misses.
//
//  At depth 1 the root's move loop takes every child to depth 0, which enters qSearch. So
//  seeding every child with an eval-less depth -5 entry, of the exact shape a null-search
//  stand-pat leaves behind, puts the search on that path many times over.
//

TEST(TranspositionTableQSearchEvalPublishTest, Positive)
{
    InitBitboards();
    Position::InitHashNumbers();
#if defined(PURE_HCE)
    Hce::init();                       //  the search below evaluates for real either way
#else
    if (!Evaluator::initEval())
        GTEST_SKIP() << "no usable embedded network; this check needs a real evaluation";
#endif

    static const char * const fen =
        "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4";

    //  collect the hash of every position one legal move from the root
    std::unique_ptr<Position> pos(new Position);
    ASSERT_EQ(true, pos->SetFEN(fen));

    MoveList ml;
    GenAllMoves(*pos, ml);

    //  A child that is IN CHECK never computes an eval - qSearch scores it from the mate
    //  distance instead - so it is not a candidate and must be excluded, or the test would
    //  demand an evaluation nobody should have made.
    std::vector<U64> childHashes;
    for (size_t i = 0; i < ml.Size(); ++i) {
        if (pos->MakeMove(ml[i].m_mv)) {
            if (!pos->InCheck())
                childHashes.push_back(pos->Hash());
            pos->UnmakeMove();
        }
    }
    ASSERT_GT(childHashes.size(), size_t(15));

    EXPECT_EQ(true, TTable::instance().setHashSize(16, 1));
    EXPECT_EQ(true, TTable::instance().clearHash(1));
    TTable::instance().clearAge();

    //  exactly what a null-search stand-pat leaves: a real score, depth -5, and NO eval
    for (U64 h : childHashes)
        TTable::instance().record(0, 20, NO_SCORE, -5, 0, HASH_BETA, false, h);

    for (U64 h : childHashes) {
        TEntry e{};
        ASSERT_EQ(true, TTable::instance().retrieve(h, e));
        ASSERT_EQ(false, isValidEval(e.eval));   // nothing cached yet
    }

    //  a real depth-1 search: every child is entered through qSearch
    std::unique_ptr<Search> search(new Search);
    ASSERT_EQ(true, search->setFEN(fen));

    Time t;
    ASSERT_EQ(true, t.parseTime({ "go", "depth", "1" }, true));
    search->startSearch(t, 1, false, true);

    //  every seeded entry the search reached must now carry an eval
    int filled = 0, seen = 0;
    for (U64 h : childHashes) {
        TEntry e{};
        if (!TTable::instance().retrieve(h, e))
            continue;
        ++seen;
        filled += isValidEval(e.eval);
    }

    EXPECT_GT(seen, 0);
    EXPECT_EQ(seen, filled);   // none of them was evaluated and then discarded
}

//
//  The producer/consumer check the helper-level tablebase test cannot make.
//
//  abSearch generates a tablebase win as TBBASE_SCORE - MAX_PLY - ply, which is relative to
//  the ROOT, and record() normalises it to be relative to the NODE. Those two cancel, so a
//  correctly recorded tablebase win is STORED as exactly TBBASE_SCORE - MAX_PLY whatever ply
//  it was found at. Passing the wrong ply to record() - the v3 defect - leaves the stored
//  value varying with depth instead. That invariant is checkable against a real search.
//
//  Needs tablebase files; skips without them, so it is a developer-machine test.
//

#if defined(SYZYGY_SUPPORT)
TEST(TranspositionTableTablebaseSearchIntegrationTest, Positive)
{
    const char * tbPath = getenv("IGEL_SYZYGY_PATH");
    if (!tbPath || !*tbPath)
        GTEST_SKIP() << "set IGEL_SYZYGY_PATH to run the tablebase integration check";

    InitBitboards();
    Position::InitHashNumbers();
#if defined(PURE_HCE)
    Hce::init();
#else
    if (!Evaluator::initEval())
        GTEST_SKIP() << "no usable embedded network; this check needs a real evaluation";
#endif

    if (!tb_init(tbPath) || TB_LARGEST == 0)
        GTEST_SKIP() << "no usable tablebases at " << tbPath;

    //  seven pieces at the root, so the ROOT probe (which bypasses the table entirely)
    //  cannot fire and every tablebase hit happens inside the tree
    static const char * const fen = "8/8/4k3/8/8/2K5/3PPP2/3R1r2 w - - 0 1";

    std::unique_ptr<Position> pos(new Position);
    ASSERT_EQ(true, pos->SetFEN(fen));
    ASSERT_GT(countBits(pos->BitsAll()), U32(TB_LARGEST));

    //  every position within two plies of the root - the tablebase entries will be among them
    std::vector<U64> hashes;
    MoveList a;
    GenAllMoves(*pos, a);
    for (size_t i = 0; i < a.Size(); ++i) {
        if (!pos->MakeMove(a[i].m_mv))
            continue;
        hashes.push_back(pos->Hash());
        MoveList b;
        GenAllMoves(*pos, b);
        for (size_t j = 0; j < b.Size(); ++j) {
            if (pos->MakeMove(b[j].m_mv)) {
                hashes.push_back(pos->Hash());
                pos->UnmakeMove();
            }
        }
        pos->UnmakeMove();
    }

    EXPECT_EQ(true, TTable::instance().setHashSize(32, 1));
    EXPECT_EQ(true, TTable::instance().clearHash(1));
    TTable::instance().clearAge();

    std::unique_ptr<Search> search(new Search);
    ASSERT_EQ(true, search->setFEN(fen));
    search->setSyzygyDepth(1);

    Time t;
    ASSERT_EQ(true, t.parseTime({ "go", "depth", "12" }, true));
    search->startSearch(t, 1, false, true);

    //
    //  The DIRECT tablebase store cancels exactly: the probe emits TBBASE_SCORE - MAX_PLY -
    //  ply and record() adds the ply back, so it lands on TBBASE_SCORE - MAX_PLY whatever
    //  depth it was found at. An ANCESTOR that merely inherits that score stores it relative
    //  to itself and so lands strictly below. The maximum across the table is therefore the
    //  normalised magnitude exactly - and with the wrong ply passed, the direct store becomes
    //  TBBASE_SCORE - MAX_PLY - ply and NOTHING can reach it.
    //

    const EVAL normalised = TBBASE_SCORE - MAX_PLY;
    EVAL       highest    = 0;
    int        seen       = 0;

    for (U64 h : hashes) {
        TEntry e{};
        if (!TTable::instance().retrieve(h, e) || !isDecisiveScore(e.score))
            continue;

        //  mates are decisive too and are normalised the same way; only tablebase
        //  magnitudes belong in this check
        if (std::abs(e.score) >= CHECKMATE_SCORE - MAX_PLY)
            continue;

        ++seen;
        highest = std::max(highest, EVAL(std::abs(e.score)));

        //  nothing may exceed it either: that would mean a ply added twice
        EXPECT_LE(std::abs(e.score), normalised);
    }

    ASSERT_GT(seen, 0) << "no tablebase entry was stored; the test proved nothing";
    EXPECT_EQ(normalised, highest)
        << "no tablebase score was stored normalised - record() got the wrong ply";
}
#endif

//
//  What the SEARCH puts in the table, not what a test puts there.
//
//  The fifty-move test above pins the contract - a raw value re-damped on use agrees with a
//  fresh evaluation - but it seeds the entry itself, so it cannot see the search caching the
//  wrong thing. Only the UNDAMPED value may be stored, because one entry serves the position
//  at every halfmove clock. Storing what evaluate() returns instead would pass every check
//  in that test and still be the original bug.
//
//  A clock of 80 makes the two differ by ~38%, so a real search over such a position must
//  leave the raw value behind and nothing else.
//

TEST(TranspositionTableSearchCachesRawEvalTest, Positive)
{
    InitBitboards();
    Position::InitHashNumbers();
#if defined(PURE_HCE)
    Hce::init();
#else
    if (!Evaluator::initEval())
        GTEST_SKIP() << "no usable embedded network; this check needs a real evaluation";
#endif

    static const char * const fen =
        "r1bq1rk1/pp2bppp/2n1pn2/2pp4/3P4/2NBPN2/PPP2PPP/R1BQ1RK1 w - - 80 52";

    Evaluator ev;
    std::unique_ptr<Position> pos(new Position);
    ASSERT_EQ(true, pos->SetFEN(fen));
    ASSERT_EQ(80, pos->Fifty());

    const EVAL raw    = ev.evaluateRaw(*pos);
    const EVAL damped = ev.evaluate(*pos);      //  what a buggy cache would store instead

#if !defined(PURE_HCE)
    //  the two must actually differ here, or this proves nothing
    ASSERT_NE(raw, damped);
#endif

    EXPECT_EQ(true, TTable::instance().setHashSize(16, 1));
    EXPECT_EQ(true, TTable::instance().clearHash(1));
    TTable::instance().clearAge();

    std::unique_ptr<Search> search(new Search);
    ASSERT_EQ(true, search->setFEN(fen));

    Time t;
    ASSERT_EQ(true, t.parseTime({ "go", "depth", "4" }, true));
    search->startSearch(t, 1, false, true);

    TEntry e{};
    ASSERT_EQ(true, TTable::instance().retrieve(pos->Hash(), e));
    ASSERT_EQ(true, isValidEval(e.eval));

    EXPECT_EQ(raw, e.eval) << "the search cached a damped evaluation; only the raw one is "
                              "valid across halfmove clocks";
#if !defined(PURE_HCE)
    EXPECT_NE(damped, e.eval);
#endif
}

//
//  CHARACTERISATION, not a desired property: this pins an ACCEPTED weakness so that the
//  documentation cannot drift from the code, and so that anyone who later adds a coherence
//  protocol sees this test fail and knows to update it.
//
//  The interleaving: writer A finds its key in a slot; writer B replaces the slot with a
//  different position; A then calls refreshEval() and lands its evaluation on B's entry.
//  The result is internally well formed - B's key, B's score, A's eval - so no range check
//  can reject it, and a single-threaded reader afterwards accepts the foreign evaluation.
//  Removing this needs a version or checksum protocol over the whole payload; a wider key
//  narrows the window without closing it.
//

TEST(TranspositionTableRefreshInterleavingTest, Characterisation)
{
    const U64  hashA = H(0x0A0A), hashB = H(0x0B0B);
    const EVAL evalA = 1111,      evalB = 2222;
    const EVAL scoreA = 11,       scoreB = 22;

    ASSERT_NE(TTEntry::keyOf(hashA), TTEntry::keyOf(hashB));

    TTEntry slot{};
    slot.store(hashA, 4242, scoreA, evalA, 8, HASH_EXACT, false, 0);
    ASSERT_EQ(evalA, slot.read().eval);

    //  ...writer B takes the slot while A is between its key test and its refresh
    slot.store(hashB, 5353, scoreB, evalB, 9, HASH_EXACT, false, 0);
    ASSERT_EQ(TTEntry::keyOf(hashB), slot.key());
    ASSERT_EQ(evalB, slot.read().eval);

    //  ...and A's refresh lands anyway
    slot.refreshEval(evalA);

    const TEntry got = slot.read();
    EXPECT_EQ(TTEntry::keyOf(hashB), slot.key());   // B's identity
    EXPECT_EQ(scoreB, got.score);                   // B's score
    EXPECT_EQ(evalA, got.eval);                     // but A's evaluation

    //  and nothing about it looks wrong to a reader, which is exactly the point
    EXPECT_EQ(true, isValidScore(got.score));
    EXPECT_EQ(true, isValidEval(got.eval));
}

TEST(TranspositionTableEntryDepthTest, Positive)
{
    TTEntry te{};

    for (auto i = DEPTH_UNSEARCHED; i <= DEPTH_MAX; ++i) {
        te.store(H(1), 1, 0, 0, i, HASH_ALPHA, false, 0);
        EXPECT_EQ(i, te.read().depth);
        EXPECT_EQ(true, te.isOccupied());
    }
}

TEST(TranspositionTableEntryMoveTest, Positive)
{
    TTEntry te{};

    InitBitboards();
    Position::InitHashNumbers();
#if !defined(PURE_HCE)
    Evaluator::initEval();   // not defined in a PURE_HCE build, and not needed there
#endif

    MoveList mvlist;
    std::unique_ptr<Position> pos(new Position);
    pos->SetInitial();

    GenAllMoves(*pos, mvlist);
    auto mvSize = mvlist.Size();

    for (size_t i = 0; i < mvSize; ++i) {
        Move mv = mvlist[i].m_mv;
        te.store(H(1), mv, 0, 0, 1, HASH_ALPHA, false, 0);
        EXPECT_EQ(mv, te.read().move);
    }

    // a move occupies 25 bits, including the castling flag in the top one
    for (U32 i = 1; i <= 33554431u; ++i) {
        te.store(H(1), Move(i), 0, 0, 1, HASH_ALPHA, false, 0);
        EXPECT_EQ(i, te.read().move);
    }
}

TEST(TranspositionTableEntryTypeTest, Positive)
{
    TTEntry te{};

    for (U8 i = 0; i <= HASH_NONE; ++i) {
        te.store(H(1), 1, 0, 0, 1, i, false, 0);
        EXPECT_EQ(i, te.read().type);
    }
}

TEST(TranspositionTableEntryPvTest, Positive)
{
    TTEntry te{};

    te.store(H(1), 1, 0, 0, 1, HASH_ALPHA, false, 0);
    EXPECT_EQ(false, te.read().pv);

    te.store(H(1), 1, 0, 0, 1, HASH_ALPHA, true, 0);
    EXPECT_EQ(true, te.read().pv);
}

TEST(TranspositionTableEntryAgeTest, Positive)
{
    TTEntry te{};

    for (U8 i = 0; i <= TTEntry::GENERATION_MASK; ++i) {
        te.store(H(1), 1, 0, 0, 1, HASH_ALPHA, false, i);
        EXPECT_EQ(i, te.age());
        EXPECT_EQ(0, te.relativeAge(i));
    }

    //
    //  generations wrap like hours on a clock
    //

    te.store(H(1), 1, 0, 0, 1, HASH_ALPHA, false, TTEntry::GENERATION_MASK);
    EXPECT_EQ(1, te.relativeAge(0));
    EXPECT_EQ(2, te.relativeAge(1));
}

//
//  Every field has to survive being written alongside every other one
//

TEST(TranspositionTableEntryCompositeTest, Positive)
{
    TTEntry te{};

    te.store(H(1), 0, 0, 0, DEPTH_NONE + 1, HASH_ALPHA, false, 0);

    EXPECT_EQ(0, te.age());
    EXPECT_EQ(HASH_ALPHA, te.read().type);
    EXPECT_EQ(0, te.read().move);
    EXPECT_EQ(DEPTH_NONE + 1, te.read().depth);
    EXPECT_EQ(0, te.read().score);
    EXPECT_EQ(0, te.read().eval);
    EXPECT_EQ(false, te.read().pv);

    for (auto i = -128; i <= 127; ++i) {
        const int depth = std::max(i, DEPTH_NONE + 1);
        te.store(H(1), 33554431u, -CHECKMATE_SCORE - 128, DECISIVE_SCORE - 1, depth, HASH_BETA, true, 7);

        const TEntry e = te.read();
        EXPECT_EQ(depth, e.depth);
        EXPECT_EQ(7, te.age());
        EXPECT_EQ(HASH_BETA, e.type);
        EXPECT_EQ(33554431u, e.move);
        EXPECT_EQ(-CHECKMATE_SCORE - 128, e.score);
        EXPECT_EQ(DECISIVE_SCORE - 1, e.eval);
        EXPECT_EQ(true, e.pv);
    }

    for (auto i = -DECISIVE_SCORE + 1; i < DECISIVE_SCORE; ++i) {
        te.store(H(1), 33554431u, i, -i, 1, HASH_EXACT, true, 7);

        const TEntry e = te.read();
        EXPECT_EQ(i, e.score);
        EXPECT_EQ(-i, e.eval);
        EXPECT_EQ(1, e.depth);
        EXPECT_EQ(7, te.age());
        EXPECT_EQ(HASH_EXACT, e.type);
        EXPECT_EQ(33554431u, e.move);
    }

    for (U8 i = 0; i <= HASH_NONE; ++i) {
        te.store(H(1), 33554431u, 1, 2, 1, i, true, 7);

        const TEntry e = te.read();
        EXPECT_EQ(i, e.type);
        EXPECT_EQ(1, e.score);
        EXPECT_EQ(2, e.eval);
        EXPECT_EQ(1, e.depth);
        EXPECT_EQ(7, te.age());
        EXPECT_EQ(33554431u, e.move);
    }

    for (U8 i = 0; i <= TTEntry::GENERATION_MASK; ++i) {
        te.store(H(1), 33554431u, 1, 2, 1, HASH_EXACT, true, i);

        const TEntry e = te.read();
        EXPECT_EQ(i, te.age());
        EXPECT_EQ(HASH_EXACT, e.type);
        EXPECT_EQ(1, e.score);
        EXPECT_EQ(2, e.eval);
        EXPECT_EQ(1, e.depth);
        EXPECT_EQ(33554431u, e.move);
    }
}

}
