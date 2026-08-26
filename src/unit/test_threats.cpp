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
#include "../notation.h"
#include "../position.h"
#include "../threats.h"

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

namespace unit
{

static const PIECE AllPieces[12] = { PW, PB, NW, NB, BW, BB, RW, RB, QW, QB, KW, KB };

static std::map<int, int> featureCounts(const ThreatList & list)
{
    std::map<int, int> counts;

    for (size_t i = 0; i < list.size(); ++i)
        counts[list[i]]++;

    return counts;
}

TEST(ThreatFeatureIndex, Positive)
{
    InitBitboards();

    const Square ksq = A1;

    std::map<std::uint32_t, std::vector<int>> seen;
    std::uint32_t highest = 0;
    bool encoded[7][7] = {};

    for (PIECE attacker : AllPieces) {

        for (FLD from = 0; from < 64; ++from) {

            // pawns never stand on the first or the last rank
            if (GetPieceType(attacker) == PAWN && (Row(from) == 0 || Row(from) == 7))
                continue;

            for (U64 attacks = Attacks(from, 0, attacker); attacks; ) {

                const FLD to = PopLSB(attacks);

                for (PIECE attacked : AllPieces) {

                    const std::uint32_t index = makeThreatIndex(WHITE, PieceAdapter[attacker], from, to,
                                                                PieceAdapter[attacked], ksq);

                    if (index >= THREAT_DIMENSIONS)
                        continue;

                    highest = std::max(highest, index);
                    encoded[GetPieceType(attacker) / 2][GetPieceType(attacked) / 2] = true;

                    const std::vector<int> threat = { attacker, from, to, attacked };
                    auto inserted = seen.emplace(index, threat);

                    EXPECT_TRUE(inserted.second || inserted.first->second == threat)
                        << "index " << index << " is shared by two different threats";
                }
            }
        }
    }

    // [attacker][attacked], indexed by piece type / 2, so pawn is 1 and king is 6
    const bool expected[7][7] = {
        { false, false, false, false, false, false, false },
        { false, true,  true,  false, true,  false, false },   // pawn
        { false, true,  true,  true,  true,  true,  false },   // knight
        { false, true,  true,  true,  true,  false, false },   // bishop
        { false, true,  true,  true,  true,  false, false },   // rook
        { false, true,  true,  true,  true,  true,  false },   // queen
        { false, false, false, false, false, false, false }    // king
    };

    for (int a = 0; a < 7; ++a)
        for (int d = 0; d < 7; ++d)
            EXPECT_EQ(expected[a][d], encoded[a][d]) << "attacker type " << a * 2 << " attacked type " << d * 2;

    EXPECT_EQ(THREAT_DIMENSIONS - 1, highest);
}

//
// Reading the board from the black point of view is the same as reading the vertically
// mirrored board with the colours swapped from the white point of view.
//

TEST(ThreatFeatureMirror, Positive)
{
    InitBitboards();

    for (PIECE attacker : AllPieces) {

        for (FLD from = 0; from < 64; ++from) {

            if (GetPieceType(attacker) == PAWN && (Row(from) == 0 || Row(from) == 7))
                continue;

            for (U64 attacks = Attacks(from, 0, attacker); attacks; ) {

                const FLD to = PopLSB(attacks);

                for (PIECE attacked : AllPieces) {

                    for (FLD ksq = 0; ksq < 64; ksq += 7) {

                        const Piece a = PieceAdapter[attacker];
                        const Piece d = PieceAdapter[attacked];

                        const std::uint32_t black = makeThreatIndex(BLACK, a, from, to, d, ksq);
                        const std::uint32_t white = makeThreatIndex(WHITE, Piece(a ^ 8), from ^ SQ_A8,
                                                                    to ^ SQ_A8, Piece(d ^ 8), ksq ^ SQ_A8);

                        EXPECT_EQ(white, black);
                    }
                }
            }
        }
    }
}

//
// The heart of the incremental update: the threats a move reports as switched off and
// on must turn the feature set that was active before the move into the one active
// after it. Checked over a full move tree so that captures, promotions, en passant,
// castling and Fischer random castling all take part.
//

static void checkThreatUpdates(Position & pos, int depth)
{
    if (depth == 0)
        return;

    MoveList mvlist;

    if (pos.InCheck())
        GenMovesInCheck(pos, mvlist);
    else
        GenAllMoves(pos, mvlist);

    const std::string fen = pos.FEN();

    ThreatList before[COLOR_NB];
    FLD        kingBefore[COLOR_NB];

    for (COLOR c = 0; c < COLOR_NB; ++c) {
        getActiveThreatIndexes(pos, c, before[c]);
        kingBefore[c] = pos.King(c);
    }

    for (size_t i = 0; i < mvlist.Size(); ++i) {

        const Move mv = mvlist[i].m_mv;

        if (!pos.MakeMove(mv))
            continue;

        EXPECT_LE(pos.state()->dirtyThreats.size(), size_t(DirtyThreats::MAX_LENGTH));

        for (COLOR c = 0; c < COLOR_NB; ++c) {

            // a move of this king refreshes its whole perspective, the dirty list
            // is not expected to describe that
            if (pos.King(c) != kingBefore[c])
                continue;

            ThreatList after, removed, added;

            getActiveThreatIndexes(pos, c, after);
            getChangedThreatIndexes(c, pos.King(c), pos.state()->dirtyThreats, removed, added);

            std::map<int, int> expected = featureCounts(before[c]);

            for (size_t k = 0; k < removed.size(); ++k)
                expected[removed[k]]--;

            for (size_t k = 0; k < added.size(); ++k)
                expected[added[k]]++;

            for (auto it = expected.begin(); it != expected.end(); )
                it = (it->second == 0) ? expected.erase(it) : std::next(it);

            EXPECT_EQ(expected, featureCounts(after))
                << "fen " << fen << " move " << MoveToStrLong(mv) << " perspective " << int(c);
        }

        checkThreatUpdates(pos, depth - 1);
        pos.UnmakeMove();
    }
}

TEST(ThreatIncrementalUpdate, Positive)
{
    InitBitboards();
    Position::InitHashNumbers();

    struct Case { const char * fen; int depth; bool frc; };

    const Case cases[] = {
        { STD_POSITION,                                                          3, false },
        { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -",     3, false },
        { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -",                               4, false },
        { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",    3, false },
        { "8/PPPk4/8/8/8/8/4Kppp/8 w - - 0 1",                                   4, false },
        { "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1",                             3, false },
        { "bqnb1rkr/pp3ppp/3ppn2/2p5/5P2/P2P4/NPP1P1PP/BQ1BNRKR w HFhf - 2 9",   3, true  },
        { "qrkrbbnn/pppppppp/8/8/8/8/PPPPPPPP/QRKRBBNN w DBdb - 0 1",            3, true  },
    };

    const bool chess960 = g_uci_chess960;

    for (const Case & c : cases) {

        g_uci_chess960 = c.frc;

        std::unique_ptr<Position> pos(new Position);
        ASSERT_EQ(true, pos->SetFEN(c.fen));

        checkThreatUpdates(*pos.get(), c.depth);
    }

    g_uci_chess960 = chess960;
}

}
