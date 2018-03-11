/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2002-2018 Vladimir Medvedev <vrm@bk.ru> (GreKo author)
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

#include "eval.h"
#include "eval_params.h"
#include "utils.h"

static const int g_pawnHashSize = 32768;
static PawnHashEntry g_pawnHash[g_pawnHashSize];

Pair PSQ[14][64];
Pair PSQ_PP_BLOCKED[64];
Pair PSQ_PP_FREE[64];

Pair PAWN_DOUBLED[64];
Pair PAWN_ISOLATED[64];
Pair KNIGHT_STRONG[64];
Pair BISHOP_PAIR;
Pair BISHOP_STRONG[64];
Pair BISHOP_MOBILITY[14];
Pair ROOK_OPEN;
Pair ROOK_7TH;
Pair QUEEN_7TH;
Pair ROOK_MOBILITY[15];
Pair QUEEN_KING_DIST[10];
Pair KING_PAWN_SHIELD[10];
Pair KING_PASSED_DIST[10];
Pair ATTACK_KING[8];
Pair ATTACK_STRONGER;

int Distance(FLD f1, FLD f2)
{
    static const int dist[100] =
    {
        0, 1, 1, 1, 2, 2, 2, 2, 2, 3,
        3, 3, 3, 3, 3, 3, 4, 4, 4, 4,
        4, 4, 4, 4, 4, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 8, 8, 8, 8, 8, 8,
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
        8, 9, 9, 9, 9, 9, 9, 9, 9, 9,
        9, 9, 9, 9, 9, 9, 9, 9, 9, 9
    };

    int drow = Row(f1) - Row(f2);
    int dcol = Col(f1) - Col(f2);
    return dist[drow * drow + dcol * dcol];
}

int PawnShieldPenalty(const PawnHashEntry& pEntry, int fileK, COLOR side)
{
    static const int delta[2][8] =
    {
        { 3, 3, 3, 3, 2, 1, 0, 3 },
        { 3, 0, 1, 2, 3, 3, 3, 3 }
    };

    int penalty = 0;
    for (int file = fileK - 1; file < fileK + 2; ++file)
    {
        int rank = pEntry.m_ranks[file][side];
        penalty += delta[side][rank];
    }

    if (penalty < 0)
        penalty = 0;
    if (penalty > 9)
        penalty = 9;

    return penalty;
}

EVAL Evaluate(const Position& pos, EVAL alpha, EVAL beta)
{
    int mid = pos.MatIndex(WHITE) + pos.MatIndex(BLACK);
    int end = 64 - mid;

    U64 x, y, occ = pos.BitsAll();
    FLD f;

    U64 KingZone[2] = { BB_KING_ATTACKS[pos.King(WHITE)], BB_KING_ATTACKS[pos.King(BLACK)] };
    int attK[2] = { 0, 0 };

    //
    //   PAWNS
    //

    int index = pos.PawnHash() % g_pawnHashSize;
    PawnHashEntry& ps = g_pawnHash[index];
    if (ps.m_pawnHash != pos.PawnHash())
        ps.Read(pos);

    Pair score = pos.Score();

    // passed
    x = ps.m_passedPawns[WHITE];
    while (x)
    {
        f = PopLSB(x);
        if (pos[f - 8] != NOPIECE)
            score += PSQ_PP_BLOCKED[f];
        else
            score += PSQ_PP_FREE[f];

        score += KING_PASSED_DIST[Distance(f - 8, pos.King(BLACK))];
        score -= KING_PASSED_DIST[Distance(f - 8, pos.King(WHITE))];
    }
    x = ps.m_passedPawns[BLACK];
    while (x)
    {
        f = PopLSB(x);
        if (pos[f + 8] != NOPIECE)
            score -= PSQ_PP_BLOCKED[FLIP[BLACK][f]];
        else
            score -= PSQ_PP_FREE[FLIP[BLACK][f]];

        score -= KING_PASSED_DIST[Distance(f + 8, pos.King(WHITE))];
        score += KING_PASSED_DIST[Distance(f + 8, pos.King(BLACK))];
    }

    // doubled
    x = ps.m_doubledPawns[WHITE];
    while (x)
    {
        f = PopLSB(x);
        score += PAWN_DOUBLED[f];
    }
    x = ps.m_doubledPawns[BLACK];
    while (x)
    {
        f = PopLSB(x);
        score -= PAWN_DOUBLED[FLIP[BLACK][f]];
    }

    // isolated
    x = ps.m_isolatedPawns[WHITE];
    while (x)
    {
        f = PopLSB(x);
        score += PAWN_ISOLATED[f];
    }
    x = ps.m_isolatedPawns[BLACK];
    while (x)
    {
        f = PopLSB(x);
        score -= PAWN_ISOLATED[FLIP[BLACK][f]];
    }

    // attacks
    {
        x = pos.Bits(PW);
        y = UpRight(x) | UpLeft(x);
        y &= (pos.Bits(NB) | pos.Bits(BB) | pos.Bits(RB) | pos.Bits(QB));
        score += CountBits(y) * ATTACK_STRONGER;
    }
    {
        x = pos.Bits(PB);
        y = DownRight(x) | DownLeft(x);
        y &= (pos.Bits(NW) | pos.Bits(BW) | pos.Bits(RW) | pos.Bits(QW));
        score -= CountBits(y) * ATTACK_STRONGER;
    }

    //
    //   KNIGHTS
    //

    if (ps.m_strongFields[WHITE])
    {
        x = ps.m_strongFields[WHITE] & pos.Bits(NW);
        while (x)
        {
            f = PopLSB(x);
            score += KNIGHT_STRONG[f];
        }
        x = ps.m_strongFields[WHITE] & pos.Bits(BW);
        while (x)
        {
            f = PopLSB(x);
            score += BISHOP_STRONG[f];
        }
    }

    if (ps.m_strongFields[BLACK])
    {
        x = ps.m_strongFields[BLACK] & pos.Bits(NB);
        while (x)
        {
            f = PopLSB(x);
            score -= KNIGHT_STRONG[FLIP[BLACK][f]];
        }
        x = ps.m_strongFields[BLACK] & pos.Bits(BB);
        while (x)
        {
            f = PopLSB(x);
            score -= BISHOP_STRONG[FLIP[BLACK][f]];
        }
    }

    x = pos.Bits(NW);
    while (x)
    {
        f = PopLSB(x);
        y = BB_KNIGHT_ATTACKS[f];
        if (y & KingZone[BLACK])
            attK[WHITE] += 1;
        y &= (pos.Bits(RB) | pos.Bits(QB));
        score += CountBits(y) * ATTACK_STRONGER;
    }

    x = pos.Bits(NB);
    while (x)
    {
        f = PopLSB(x);
        y = BB_KNIGHT_ATTACKS[f];
        if (y & KingZone[WHITE])
            attK[BLACK] += 1;
        y &= (pos.Bits(RW) | pos.Bits(QW));
        score -= CountBits(y) * ATTACK_STRONGER;
    }

    //
    //   BISHOPS
    //

    if (pos.Count(BW) == 2)
        score += BISHOP_PAIR;
    if (pos.Count(BB) == 2)
        score -= BISHOP_PAIR;

    x = pos.Bits(BW);
    while (x)
    {
        f = PopLSB(x);
        y = BishopAttacks(f, occ);
        score += BISHOP_MOBILITY[CountBits(y)];
        if (y & KingZone[BLACK])
            attK[WHITE] += 1;
        y &= (pos.Bits(RB) | pos.Bits(QB));
        score += CountBits(y) * ATTACK_STRONGER;
    }

    x = pos.Bits(BB);
    while (x)
    {
        f = PopLSB(x);
        y = BishopAttacks(f, occ);
        score -= BISHOP_MOBILITY[CountBits(y)];
        if (y & KingZone[WHITE])
            attK[BLACK] += 1;
        y &= (pos.Bits(RW) | pos.Bits(QW));
        score -= CountBits(y) * ATTACK_STRONGER;
    }

    //
    //   ROOKS
    //

    x = pos.Bits(RW);
    while (x)
    {
        f = PopLSB(x);
        y = RookAttacks(f, occ);
        score += ROOK_MOBILITY[CountBits(y)];

        if (y & KingZone[BLACK])
            attK[WHITE] += 1;

        y &= pos.Bits(QB);
        score += CountBits(y) * ATTACK_STRONGER;

        int file = Col(f) + 1;
        if (ps.m_ranks[file][WHITE] == 0)
            score += ROOK_OPEN;

        if (Row(f) == 1)
        {
            if ((pos.Bits(PB) & LL(0x00ff000000000000)) || Row(pos.King(BLACK)) == 0)
                score += ROOK_7TH;
        }
    }

    x = pos.Bits(RB);
    while (x)
    {
        f = PopLSB(x);
        y = RookAttacks(f, occ);
        score -= ROOK_MOBILITY[CountBits(y)];

        if (y & KingZone[WHITE])
            attK[BLACK] += 1;

        y &= pos.Bits(QW);
        score -= CountBits(y) * ATTACK_STRONGER;

        int file = Col(f) + 1;
        if (ps.m_ranks[file][BLACK] == 7)
            score -= ROOK_OPEN;

        if (Row(f) == 6)
        {
            if ((pos.Bits(PW) & LL(0x000000000000ff00)) || Row(pos.King(WHITE)) == 7)
                score -= ROOK_7TH;
        }
    }

    //
    //   QUEENS
    //

    x = pos.Bits(QW);
    while (x)
    {
        f = PopLSB(x);
        int dist = Distance(f, pos.King(BLACK));
        score += QUEEN_KING_DIST[dist];
        y = QueenAttacks(f, occ);
        if (y & KingZone[BLACK])
            attK[WHITE] += 1;

        if (Row(f) == 1)
        {
            if ((pos.Bits(PB) & LL(0x00ff000000000000)) || Row(pos.King(BLACK)) == 0)
                score += QUEEN_7TH;
        }
    }

    x = pos.Bits(QB);
    while (x)
    {
        f = PopLSB(x);
        int dist = Distance(f, pos.King(WHITE));
        score -= QUEEN_KING_DIST[dist];
        y = QueenAttacks(f, occ);
        if (y & KingZone[WHITE])
            attK[BLACK] += 1;

        if (Row(f) == 6)
        {
            if ((pos.Bits(PW) & LL(0x000000000000ff00)) || Row(pos.King(WHITE)) == 7)
                score -= QUEEN_7TH;
        }
    }

    //
    //   KINGS
    //

    {
        f = pos.King(WHITE);
        int fileK = Col(f) + 1;
        int shield = PawnShieldPenalty(ps, fileK, WHITE);
        score += KING_PAWN_SHIELD[shield];
    }
    {
        f = pos.King(BLACK);
        int fileK = Col(f) + 1;
        int shield = PawnShieldPenalty(ps, fileK, BLACK);
        score -= KING_PAWN_SHIELD[shield];
    }

    //
    //   ATTACKS
    //

    if (attK[WHITE] > 3)
        attK[WHITE] = 3;

    if (attK[BLACK] > 3)
        attK[BLACK] = 3;

    score += ATTACK_KING[attK[WHITE]];
    score -= ATTACK_KING[attK[BLACK]];

    EVAL e = (score.mid * mid + score.end * end) / 64;

    if (e > 0 && pos.MatIndex(WHITE) < 5 && pos.Count(PW) == 0)
        e = 0;
    if (e < 0 && pos.MatIndex(BLACK) < 5 && pos.Count(PB) == 0)
        e = 0;

    if (pos.Side() == BLACK)
        e = -e;

    return e;
}

int Q2(int tag, double x, double y)
{
    // A*x*x + B*x + C*y*y + D*y + E*x*y + F

    int* ptr = &(W[lines[tag].start]);

    int A = ptr[0];
    int B = ptr[1];
    int C = ptr[2];
    int D = ptr[3];
    int E = ptr[4];
    int F = ptr[5];

    double val = A * x * x + B * x + C * y * y + D * y + E * x * y + F;
    return int(val);
}

int Q1(int tag, double x)
{
    // A*x*x + B*x + C

    int* ptr = &(W[lines[tag].start]);

    int A = ptr[0];
    int B = ptr[1];
    int C = ptr[2];

    double val = A * x * x + B * x + C;
    return int(val);
}

void InitEval(const vector<int>& x)
{
    W = x;

    #define P(name, index) W[lines[name].start + index]

    for (FLD f = 0; f < 64; ++f)
    {
        double x = ((Col(f) > 3)? Col(f) - 3.5 : 3.5 - Col(f)) / 3.5;
        double y = (3.5 - Row(f)) / 3.5;

        if (Row(f) != 0 && Row(f) != 7)
        {
            PSQ[PW][f].mid = VAL_P + Q2(Mid_Pawn, x, y);
            PSQ[PW][f].end = VAL_P + Q2(End_Pawn, x, y);

            PSQ_PP_BLOCKED[f].mid = Q2(Mid_PawnPassedBlocked, x, y);
            PSQ_PP_BLOCKED[f].end = Q2(End_PawnPassedBlocked, x, y);

            PSQ_PP_FREE[f].mid = Q2(Mid_PawnPassedFree, x, y);
            PSQ_PP_FREE[f].end = Q2(End_PawnPassedFree, x, y);

            PAWN_DOUBLED[f].mid = Q2(Mid_PawnDoubled, x, y);
            PAWN_DOUBLED[f].end = Q2(End_PawnDoubled, x, y);

            PAWN_ISOLATED[f].mid = Q2(Mid_PawnIsolated, x, y);
            PAWN_ISOLATED[f].end = Q2(End_PawnIsolated, x, y);
        }
        else
        {
            PSQ[PW][f] = VAL_P;
            PSQ_PP_BLOCKED[f] = 0;
            PSQ_PP_FREE[f] = 0;
            PAWN_DOUBLED[f] = 0;
            PAWN_ISOLATED[f] = 0;
        }

        PSQ[NW][f].mid = VAL_N + Q2(Mid_Knight, x, y);
        PSQ[NW][f].end = VAL_N + Q2(End_Knight, x, y);

        PSQ[BW][f].mid = VAL_B + Q2(Mid_Bishop, x, y);
        PSQ[BW][f].end = VAL_B + Q2(End_Bishop, x, y);

        PSQ[RW][f].mid = VAL_R + Q2(Mid_Rook, x, y);
        PSQ[RW][f].end = VAL_R + Q2(End_Rook, x, y);

        PSQ[QW][f].mid = VAL_Q + Q2(Mid_Queen, x, y);
        PSQ[QW][f].end = VAL_Q + Q2(End_Queen, x, y);

        PSQ[KW][f].mid = VAL_K + Q2(Mid_King, x, y);
        PSQ[KW][f].end = VAL_K + Q2(End_King, x, y);

        PSQ[PB][FLIP[BLACK][f]] = -PSQ[PW][f];
        PSQ[NB][FLIP[BLACK][f]] = -PSQ[NW][f];
        PSQ[BB][FLIP[BLACK][f]] = -PSQ[BW][f];
        PSQ[RB][FLIP[BLACK][f]] = -PSQ[RW][f];
        PSQ[QB][FLIP[BLACK][f]] = -PSQ[QW][f];
        PSQ[KB][FLIP[BLACK][f]] = -PSQ[KW][f];

        KNIGHT_STRONG[f].mid = Q2(Mid_KnightStrong, x, y);
        KNIGHT_STRONG[f].end = Q2(End_KnightStrong, x, y);

        BISHOP_STRONG[f].mid = Q2(Mid_BishopStrong, x, y);
        BISHOP_STRONG[f].end = Q2(End_BishopStrong, x, y);
    }

    BISHOP_PAIR = Pair(P(Mid_BishopPair, 0), P(End_BishopPair, 0));

    BISHOP_MOBILITY[0] = 0;
    ROOK_MOBILITY[0] = 0;
    ROOK_MOBILITY[1] = 0;

    for (int m = 0; m < 13; ++m)
    {
        double z = (m - 6.5) / 6.5;
        BISHOP_MOBILITY[m + 1].mid = Q1(Mid_BishopMobility, z);
        BISHOP_MOBILITY[m + 1].end = Q1(End_BishopMobility, z);

        ROOK_MOBILITY[m + 2].mid = Q1(Mid_RookMobility, z);
        ROOK_MOBILITY[m + 2].end = Q1(End_RookMobility, z);
    }

    ROOK_OPEN = Pair(P(Mid_RookOpen, 0), P(End_RookOpen, 0));
    ROOK_7TH = Pair(P(Mid_Rook7th, 0), P(End_Rook7th, 0));

    QUEEN_7TH = Pair(P(Mid_Queen7th, 0), P(End_Queen7th, 0));

    QUEEN_KING_DIST[0] = 0;
    QUEEN_KING_DIST[1] = 0;
    for (int d = 0; d < 8; ++d)
    {
        double z = (d - 3.5) / 3.5;
        QUEEN_KING_DIST[d + 2].mid = Q1(Mid_QueenKingDist, z);
        QUEEN_KING_DIST[d + 2].end = Q1(End_QueenKingDist, z);
    }

    for (int d = 0; d < 10; ++d)
    {
        double z = (d - 4.5) / 4.5;
        KING_PASSED_DIST[d].mid = Q1(Mid_KingPassedDist, z);
        KING_PASSED_DIST[d].end = Q1(End_KingPassedDist, z);
    }

    for (int p = 0; p < 10; ++p)
    {
        double z = (p - 4.5) / 4.5;
        KING_PAWN_SHIELD[p].mid = Q1(Mid_KingPawnShield, z);
        KING_PAWN_SHIELD[p].end = Q1(End_KingPawnShield, z);
    }

    ATTACK_STRONGER = Pair(P(Mid_AttackStronger, 0), P(End_AttackStronger, 0));

    for (int att = 0; att < 4; ++att)
        ATTACK_KING[att] = Pair(P(Mid_AttackKingZone, att), P(End_AttackKingZone, att));
}

void InitEval()
{
    InitParamLines();
    SetDefaultValues(W);
    InitEval(W);
}

void PawnHashEntry::Read(const Position& pos)
{
    m_pawnHash = pos.PawnHash();

    m_passedPawns[WHITE] = m_passedPawns[BLACK] = 0;
    m_doubledPawns[WHITE] = m_doubledPawns[BLACK] = 0;
    m_isolatedPawns[WHITE] = m_isolatedPawns[BLACK] = 0;
    m_strongFields[WHITE] = m_strongFields[BLACK] = 0;

    for (int file = 0; file < 10; ++file)
    {
        m_ranks[file][WHITE] = 0;
        m_ranks[file][BLACK] = 7;
    }

    U64 x, y;
    FLD f;

    // first pass

    x = pos.Bits(PW);
    m_strongFields[WHITE] = UpRight(x) | UpLeft(x);

    while (x)
    {
        f = PopLSB(x);
        int file = Col(f) + 1;
        int rank = Row(f);
        if (rank > m_ranks[file][WHITE])
            m_ranks[file][WHITE] = rank;
    }

    x = pos.Bits(PB);
    m_strongFields[BLACK] = DownRight(x) | DownLeft(x);

    while (x)
    {
        f = PopLSB(x);
        int file = Col(f) + 1;
        int rank = Row(f);
        if (rank < m_ranks[file][BLACK])
            m_ranks[file][BLACK] = rank;
    }

    // second pass

    x = pos.Bits(PW);
    while (x)
    {
        f = PopLSB(x);
        int file = Col(f) + 1;
        int rank = Row(f);
        if (rank <= m_ranks[file][WHITE] && rank < m_ranks[file][BLACK])
        {
            if (rank <= m_ranks[file - 1][BLACK] && rank <= m_ranks[file + 1][BLACK])
                m_passedPawns[WHITE] |= BB_SINGLE[f];
        }
        if (rank != m_ranks[file][WHITE])
            m_doubledPawns[WHITE] |= BB_SINGLE[f];
        if (m_ranks[file - 1][WHITE] == 0 && m_ranks[file + 1][WHITE] == 0)
            m_isolatedPawns[WHITE] |= BB_SINGLE[f];

        y = BB_DIR[f][DIR_U];
        y = Left(y) | Right(y);
        m_strongFields[BLACK] &= ~y;
    }

    x = pos.Bits(PB);
    while (x)
    {
        f = PopLSB(x);
        int file = Col(f) + 1;
        int rank = Row(f);
        if (rank >= m_ranks[file][BLACK] && rank > m_ranks[file][WHITE])
        {
            if (rank >= m_ranks[file - 1][WHITE] && rank >= m_ranks[file + 1][WHITE])
                m_passedPawns[BLACK] |= BB_SINGLE[f];
        }
        if (rank != m_ranks[file][BLACK])
            m_doubledPawns[BLACK] |= BB_SINGLE[f];
        if (m_ranks[file - 1][BLACK] == 7 && m_ranks[file + 1][BLACK] == 7)
            m_isolatedPawns[BLACK] |= BB_SINGLE[f];

        y = BB_DIR[f][DIR_D];
        y = Left(y) | Right(y);
        m_strongFields[WHITE] &= ~y;
    }
}

