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
#include "../notation.h"
#include "../position.h"
#include <gtest/gtest.h>
#include <memory>

namespace unit
{

namespace
{

struct CorrectionKeys
{
    U64 pawn;
    U64 minor;
    U64 nonPawn[2];

    bool operator==(const CorrectionKeys & other) const {
        return pawn == other.pawn
            && minor == other.minor
            && nonPawn[WHITE] == other.nonPawn[WHITE]
            && nonPawn[BLACK] == other.nonPawn[BLACK];
    }
};

CorrectionKeys keysOf(const Position & pos) {
    return { pos.PawnHash(), pos.MinorHash(), { pos.NonPawnHash(WHITE), pos.NonPawnHash(BLACK) } };
}

//
//  A board built from the same fen puts every piece down one by one, so its keys are the
//  from scratch answer that the incremental updates of make and unmake have to match
//

CorrectionKeys keysFromScratch(Position & scratch, const Position & pos) {
    EXPECT_TRUE(scratch.SetFEN(pos.FEN()));
    return keysOf(scratch);
}

void walk(Position & pos, Position & scratch, int depth) {
    EXPECT_TRUE(keysOf(pos) == keysFromScratch(scratch, pos)) << "keys drifted at " << pos.FEN();

    if (depth == 0)
        return;

    MoveList mvlist;

    if (pos.InCheck())
        GenMovesInCheck(pos, mvlist);
    else
        GenAllMoves(pos, mvlist);

    auto mvSize = mvlist.Size();

    for (size_t i = 0; i < mvSize; ++i)
    {
        Move mv = mvlist[i].m_mv;
        const auto before = keysOf(pos);

        if (!pos.MakeMove(mv))
            continue;

        walk(pos, scratch, depth - 1);
        pos.UnmakeMove();

        EXPECT_TRUE(keysOf(pos) == before) << "unmake of " << MoveToStrLong(mv) << " left " << pos.FEN();
    }
}

void walkFromFen(const char * fen, int depth, bool frc = false) {
    InitBitboards();
    Position::InitHashNumbers();

    const bool saved = g_uci_chess960;
    g_uci_chess960 = frc;

    auto pos = std::make_unique<Position>();
    auto scratch = std::make_unique<Position>();

    ASSERT_TRUE(pos->SetFEN(fen));
    walk(*pos, *scratch, depth);

    g_uci_chess960 = saved;
}

CorrectionKeys keysAfter(const char * fen, const char * move, bool frc = false) {
    InitBitboards();
    Position::InitHashNumbers();

    const bool saved = g_uci_chess960;
    g_uci_chess960 = frc;

    auto pos = std::make_unique<Position>();
    EXPECT_TRUE(pos->SetFEN(fen));

    Move mv = StrToMove(move, *pos);
    EXPECT_TRUE(mv != 0) << "illegal move " << move << " in " << fen;
    EXPECT_TRUE(pos->MakeMove(mv));

    const auto keys = keysOf(*pos);
    g_uci_chess960 = saved;

    return keys;
}

CorrectionKeys keysOfFen(const char * fen, bool frc = false) {
    InitBitboards();
    Position::InitHashNumbers();

    const bool saved = g_uci_chess960;
    g_uci_chess960 = frc;

    auto pos = std::make_unique<Position>();
    EXPECT_TRUE(pos->SetFEN(fen));

    const auto keys = keysOf(*pos);
    g_uci_chess960 = saved;

    return keys;
}

}

//
//  The keys have to survive a whole subtree of make and unmake, including the promotions,
//  en passant captures and both flavours of castling the walks below reach
//

TEST(CorrectionKeys, MatchAFreshBoardThroughTheOpening) {
    walkFromFen(STD_POSITION, 4);
}

TEST(CorrectionKeys, MatchAFreshBoardWithEveryPieceInPlay) {
    walkFromFen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3);
}

TEST(CorrectionKeys, MatchAFreshBoardAroundPromotions) {
    walkFromFen("n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1", 4);
}

TEST(CorrectionKeys, MatchAFreshBoardAroundEnPassant) {
    walkFromFen("rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3", 3);
}

TEST(CorrectionKeys, MatchAFreshBoardInChess960) {
    walkFromFen("bqnb1rkr/pp3ppp/3ppn2/2p5/5P2/P2P4/NPP1P1PP/BQ1BNRKR w HFhf - 2 9", 3, true);
    walkFromFen("rk2r3/8/8/8/8/8/8/RK2R3 w AEae - 0 1", 3, true);
}

//
//  Each key stands for one group of pieces, and a move only belongs to the groups it moved
//

TEST(CorrectionKeys, APawnMoveOnlyChangesThePawnKey) {
    const auto before = keysOfFen(STD_POSITION);
    const auto after  = keysAfter(STD_POSITION, "e2e4");

    EXPECT_NE(before.pawn, after.pawn);
    EXPECT_EQ(before.minor, after.minor);
    EXPECT_EQ(before.nonPawn[WHITE], after.nonPawn[WHITE]);
    EXPECT_EQ(before.nonPawn[BLACK], after.nonPawn[BLACK]);
}

TEST(CorrectionKeys, AKnightMoveChangesTheMinorAndOwnNonPawnKeys) {
    const auto before = keysOfFen(STD_POSITION);
    const auto after  = keysAfter(STD_POSITION, "g1f3");

    EXPECT_EQ(before.pawn, after.pawn);
    EXPECT_NE(before.minor, after.minor);
    EXPECT_NE(before.nonPawn[WHITE], after.nonPawn[WHITE]);
    EXPECT_EQ(before.nonPawn[BLACK], after.nonPawn[BLACK]);
}

TEST(CorrectionKeys, ARookMoveLeavesTheMinorKeyAlone) {
    const char * fen = "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1";
    const auto before = keysOfFen(fen);
    const auto after  = keysAfter(fen, "a1c1");

    EXPECT_EQ(before.pawn, after.pawn);
    EXPECT_EQ(before.minor, after.minor);
    EXPECT_NE(before.nonPawn[WHITE], after.nonPawn[WHITE]);
    EXPECT_EQ(before.nonPawn[BLACK], after.nonPawn[BLACK]);
}

TEST(CorrectionKeys, AKingIsNonPawnMaterialButNotAMinor) {
    const char * fen = "4k3/8/8/8/8/8/8/4K3 w - - 0 1";
    const auto before = keysOfFen(fen);
    const auto after  = keysAfter(fen, "e1e2");

    EXPECT_EQ(before.pawn, after.pawn);
    EXPECT_EQ(before.minor, after.minor);
    EXPECT_NE(before.nonPawn[WHITE], after.nonPawn[WHITE]);
    EXPECT_EQ(before.nonPawn[BLACK], after.nonPawn[BLACK]);
}

TEST(CorrectionKeys, APromotionMovesThePawnOutOfThePawnKey) {
    const char * fen = "8/P3k3/8/8/8/8/8/4K3 w - - 0 1";
    const auto before = keysOfFen(fen);
    const auto after  = keysAfter(fen, "a7a8q");

    EXPECT_NE(before.pawn, after.pawn);
    EXPECT_EQ(before.minor, after.minor);       // a queen is not a minor piece
    EXPECT_NE(before.nonPawn[WHITE], after.nonPawn[WHITE]);
    EXPECT_EQ(before.nonPawn[BLACK], after.nonPawn[BLACK]);

    //
    //  the pawn that left is the only pawn on the board, so what is left is the empty key
    //

    EXPECT_EQ(keysOfFen("8/4k3/8/8/8/8/8/4K3 w - - 0 1").pawn, after.pawn);
}

TEST(CorrectionKeys, AnUnderPromotionToAKnightEntersTheMinorKey) {
    const char * fen = "8/P3k3/8/8/8/8/8/4K3 w - - 0 1";
    const auto before = keysOfFen(fen);
    const auto after  = keysAfter(fen, "a7a8n");

    EXPECT_NE(before.minor, after.minor);
    EXPECT_NE(before.nonPawn[WHITE], after.nonPawn[WHITE]);
}

TEST(CorrectionKeys, CapturingAKnightWithAPawnChangesEveryKeyItBelongedTo) {
    const char * fen = "4k3/8/8/8/4n3/3P4/8/4K3 w - - 0 1";
    const auto before = keysOfFen(fen);
    const auto after  = keysAfter(fen, "d3e4");

    EXPECT_NE(before.pawn, after.pawn);
    EXPECT_NE(before.minor, after.minor);
    EXPECT_EQ(before.nonPawn[WHITE], after.nonPawn[WHITE]);
    EXPECT_NE(before.nonPawn[BLACK], after.nonPawn[BLACK]);
}

TEST(CorrectionKeys, AnEnPassantCaptureRemovesTheCapturedPawn) {
    const auto after = keysAfter("4k3/8/8/2pP4/8/8/8/4K3 w - c6 0 2", "d5c6");

    //
    //  the same board reached without an en passant, so both pawns must be accounted for
    //

    EXPECT_EQ(keysOfFen("4k3/8/2P5/8/8/8/8/4K3 b - - 0 2").pawn, after.pawn);
}

//
//  The point of the keys is that unrelated positions share them: the correction learnt for
//  one pawn structure has to be readable from every other position that reaches it
//

TEST(CorrectionKeys, TheSamePawnStructureSharesAPawnKey) {
    const auto rooks   = keysOfFen("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1");
    const auto knights = keysOfFen("n3k2n/pppppppp/8/8/8/8/PPPPPPPP/N3K2N w - - 0 1");

    EXPECT_EQ(rooks.pawn, knights.pawn);
    EXPECT_NE(rooks.minor, knights.minor);
    EXPECT_NE(rooks.nonPawn[WHITE], knights.nonPawn[WHITE]);
}

TEST(CorrectionKeys, EachSideKeepsItsOwnNonPawnKey) {
    const auto white = keysOfFen("4k3/8/8/8/8/8/8/R3K3 w - - 0 1");
    const auto black = keysOfFen("r3k3/8/8/8/8/8/8/4K3 w - - 0 1");

    EXPECT_NE(white.nonPawn[WHITE], black.nonPawn[WHITE]);
    EXPECT_NE(white.nonPawn[BLACK], black.nonPawn[BLACK]);
}

}
