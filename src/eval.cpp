/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2002-2018 Vladimir Medvedev <vrm@bk.ru> (GreKo author)
*  Copyright (C) 2018-2019 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

Pair PSQ[14][64];

//
//  Pawn evaluations
//

Pair passedPawn[64];
Pair passedPawnBlocked[64];
Pair passedPawnFree[64];
Pair passedPawnConnected[64];
Pair pawnDoubled[64];
Pair pawnIsolated[64];
Pair pawnDoubledIsolated[64];
Pair pawnBlocked[64];
Pair pawnFence[64];
Pair pawnOnBiColor;

//
//  Mobility evaluations
//

Pair knightMobility[9];
Pair bishopMobility[14];
Pair rookMobility[15];
Pair queenMobility[28];

//
//  Positional evaluation
//

Pair knightStrong[64];
Pair knightForepost[64];
Pair knightKingDistance[10];
Pair bishopStrong[64];
Pair bishopKingDistance[10];
Pair rookOnOpenFile;
Pair rookOn7thRank;
Pair rookKingDistance[10];
Pair queenOn7thRank;
Pair queenKingDistance[10];
Pair rooksConnected;

//
//  King safety evaluation
//

Pair kingPawnShield[10];
Pair kingPawnStorm[10];
Pair kingPasserDistance[10];
Pair kingExplosed[28];
Pair attackKing[8];
Pair strongAttack;
Pair centerAttack;
Pair tempoAttack;

//
//  Piece combinations evaluations
//

Pair rooksPair;
Pair bishopsPair;
Pair knightsPair;
Pair knightAndQueen;
Pair bishopAndRook;

/*static */U64 Evaluator::m_pieceAttacks[KB];

/*static */EVAL Evaluator::evaluate(Position & pos)
{
    U64 kingZone[2] = { BB_KING_ATTACKS[pos.King(WHITE)], BB_KING_ATTACKS[pos.King(BLACK)] };
    int attackKing[2] = { 0, 0 };

    PawnHashEntry *ps = nullptr;
    memset(m_pieceAttacks, 0, sizeof(m_pieceAttacks));

    U64 occ = pos.BitsAll();
    Pair score = pos.Score();

    score += evaluatePawns(pos, occ, &ps);
    score += evaluatePawnsAttacks(pos);
    score += evaluateOutposts(pos, ps);
    score += evaluateKnights(pos, kingZone, attackKing);
    score += evaluateBishops(pos, occ, kingZone, attackKing);
    score += evaluateRooks(pos, occ, kingZone, attackKing, ps);
    score += evaluateQueens(pos, occ, kingZone, attackKing);
    score += evaluateKings(pos, occ, ps);
    score += evaluatePiecePairs(pos);
    score += evaluateKingAttackers(pos, attackKing);

    int mid = pos.MatIndex(WHITE) + pos.MatIndex(BLACK);
    int end = 64 - mid;

    EVAL eval = (score.mid * mid + score.end * end) / 64;

    if (pos.Side() == BLACK)
        eval = -eval;

    return eval;
}

/*static */Pair Evaluator::evaluatePawns(Position & pos, U64 occ, PawnHashEntry ** pps)
{
    U64 x;
    FLD f;
    Pair score;

    int index = pos.PawnHash() % pos.m_pawnHashSize;
    PawnHashEntry& ps = pos.m_pawnHashTable[index];
    if (ps.m_pawnHash != pos.PawnHash())
        ps.Read(pos);

    *pps = &ps;

    U64 pawns = pos.Bits(PW);
    m_pieceAttacks[WhiteAttacks] = m_pieceAttacks[PW] = (((pawns << 9) & L1MASK) | ((pawns << 7) & R1MASK));

    // passed
    x = ps.m_passedPawns[WHITE];

    while (x)
    {
        f = PopLSB(x);

        score += passedPawn[f];

        auto freeFile = [](FLD field, U64 occ, Position& pos) -> bool
        {
            int rank = Row(field);
            int end_of_file = field - (8 * rank);
            return ((BB_BETWEEN[field][end_of_file] & occ) == 0 && pos[end_of_file] == NOPIECE);
        };

        // check if it is a blocked or a free pass pawn
        if (pos[f - 8] != NOPIECE)
            score += passedPawnBlocked[f];
        else if ((BB_DIR[f][DIR_U] & occ) == 0)
            score += passedPawnFree[f];

        // check if it is a connected pass pawn
        if ((BB_PAWN_CONNECTED[f] & ps.m_passedPawns[WHITE]) && freeFile(f, occ, pos))
            score += passedPawnConnected[f];

        score += kingPasserDistance[distance(f - 8, pos.King(BLACK))];
        score -= kingPasserDistance[distance(f - 8, pos.King(WHITE))];
    }

    pawns = pos.Bits(PB);
    m_pieceAttacks[BlackAttacks] = m_pieceAttacks[PB] = (((pawns >> 9) & R1MASK) | ((pawns >> 7) & L1MASK));

    x = ps.m_passedPawns[BLACK];
    while (x)
    {
        f = PopLSB(x);
        score -= passedPawn[FLIP[BLACK][f]];

        auto freeFile = [](FLD field, U64 occ, Position& pos) -> bool
        {
            int rank = Row(field);
            int end_of_file = field + ((7 - rank) * 8);
            return ((BB_BETWEEN[field][end_of_file] & occ) == 0 && pos[end_of_file] == NOPIECE);
        };

        // check if it is a blocked or a free pass pawn
        if (pos[f + 8] != NOPIECE)
            score -= passedPawnBlocked[FLIP[BLACK][f]];
        else if ((BB_DIR[f][DIR_D] & occ) == 0)
            score -= passedPawnFree[FLIP[BLACK][f]];

        // check if it is a connected pass pawn
        if ((BB_PAWN_CONNECTED[f] & ps.m_passedPawns[BLACK]) && freeFile(f, occ, pos))
            score -= passedPawnConnected[FLIP[BLACK][f]];

        score -= kingPasserDistance[distance(f + 8, pos.King(WHITE))];
        score += kingPasserDistance[distance(f + 8, pos.King(BLACK))];
    }

    // doubled
    x = ps.m_doubledPawns[WHITE];
    while (x)
    {
        f = PopLSB(x);
        score += pawnDoubled[f];
    }
    x = ps.m_doubledPawns[BLACK];
    while (x)
    {
        f = PopLSB(x);
        score -= pawnDoubled[FLIP[BLACK][f]];
    }

    // isolated
    x = ps.m_isolatedPawns[WHITE];
    while (x)
    {
        f = PopLSB(x);
        score += pawnIsolated[f];
    }
    x = ps.m_isolatedPawns[BLACK];
    while (x)
    {
        f = PopLSB(x);
        score -= pawnIsolated[FLIP[BLACK][f]];
    }

    // doubled and isolated
    x = ps.m_doubledPawns[WHITE] & ps.m_isolatedPawns[WHITE];
    while (x)
    {
        f = PopLSB(x);
        score += pawnDoubledIsolated[f];
    }
    x = ps.m_doubledPawns[BLACK] & ps.m_isolatedPawns[BLACK];
    while (x)
    {
        f = PopLSB(x);
        score -= pawnDoubledIsolated[FLIP[BLACK][f]];
    }

    // blocked
    x = pos.Bits(PW) & Down(occ);
    while (x)
    {
        f = PopLSB(x);
        score += pawnBlocked[f];
    }
    x = pos.Bits(PB) & Up(occ);
    while (x)
    {
        f = PopLSB(x);
        score -= pawnBlocked[FLIP[BLACK][f]];
    }

    // fence
    x = pos.Bits(PW) & Down(pos.Bits(PB));
    while (x)
    {
        f = PopLSB(x);
        score += pawnFence[f];
    }
    x = pos.Bits(PB) & Up(pos.Bits(PW));
    while (x)
    {
        f = PopLSB(x);
        score -= pawnFence[FLIP[BLACK][f]];
    }

    // pawns on a bishop color
    if (pos.Count(BW) == 1)
    {
        U64 mask = (pos.Bits(BW) & BB_WHITE_FIELDS) ? BB_WHITE_FIELDS : BB_BLACK_FIELDS;
        score += countBits(pos.Bits(PW) & mask) * pawnOnBiColor;
    }

    if (pos.Count(BB) == 1)
    {
        U64 mask = (pos.Bits(BB) & BB_WHITE_FIELDS) ? BB_WHITE_FIELDS : BB_BLACK_FIELDS;
        score -= countBits(pos.Bits(PB) & mask) * pawnOnBiColor;
    }

    return score;
}

/*static */Pair Evaluator::evaluatePawnsAttacks(Position & pos)
{
    Pair score;
    U64 x, y;

    x = pos.Bits(PW);
    y = UpRight(x) | UpLeft(x);
    y &= (pos.Bits(NB) | pos.Bits(BB) | pos.Bits(RB) | pos.Bits(QB));
    score += countBits(y) * strongAttack;

    U64 y2 = y & BB_CENTER[WHITE];
    score += countBits(y2) * centerAttack;

    x = pos.Bits(PB);
    y = DownRight(x) | DownLeft(x);
    y &= (pos.Bits(NW) | pos.Bits(BW) | pos.Bits(RW) | pos.Bits(QW));
    score -= countBits(y) * strongAttack;

    y2 = y & BB_CENTER[BLACK];
    score -= countBits(y2) * centerAttack;

    return score;
}

/*static */Pair Evaluator::evaluateOutposts(Position & pos, PawnHashEntry *ps)
{
    Pair score;
    U64 x;
    FLD f;

    if (ps->m_strongFields[WHITE])
    {
        x = ps->m_strongFields[WHITE] & pos.Bits(NW);
        while (x)
        {
            f = PopLSB(x);
            score += knightStrong[f];

            int file = Col(f) + 1;
            if (ps->m_ranks[file][WHITE] == 0)
            {
                if (BB_DIR[f][DIR_D] & pos.Bits(NW))
                    score += knightForepost[f];
            }
        }
        x = ps->m_strongFields[WHITE] & pos.Bits(BW);
        while (x)
        {
            f = PopLSB(x);
            score += bishopStrong[f];
        }
    }

    if (ps->m_strongFields[BLACK])
    {
        x = ps->m_strongFields[BLACK] & pos.Bits(NB);
        while (x)
        {
            f = PopLSB(x);
            score -= knightStrong[FLIP[BLACK][f]];

            int file = Col(f) + 1;
            if (ps->m_ranks[file][BLACK] == 7)
            {
                if (BB_DIR[f][DIR_U] & pos.Bits(RB))
                    score -= knightForepost[FLIP[BLACK][f]];
            }
        }
        x = ps->m_strongFields[BLACK] & pos.Bits(BB);
        while (x)
        {
            f = PopLSB(x);
            score -= bishopStrong[FLIP[BLACK][f]];
        }
    }

    return score;
}

/*static */Pair Evaluator::evaluateKnights(Position& pos, U64 kingZone[], int attackers[])
{
    Pair score;
    U64 x, y, mobility;
    FLD f;

    x = pos.Bits(NW);
    while (x)
    {
        f = PopLSB(x);

        int dist = distance(f, pos.King(BLACK));
        score += knightKingDistance[dist];

        mobility = y = BB_KNIGHT_ATTACKS[f];
        m_pieceAttacks[WhiteAttacks] |= m_pieceAttacks[NW] |= y;

        if (y & kingZone[BLACK])
            attackers[WHITE]++;

        mobility &= ~pos.BitsAll(); // exclude enemy/friendly occupied squares
        mobility &= ~m_pieceAttacks[PB]; // exclude attacks by enemy pawns
        score += knightMobility[countBits(mobility)];

        y &= (pos.Bits(RB) | pos.Bits(QB));
        score += countBits(y) * strongAttack;

        U64 y2 = y & BB_CENTER[WHITE];
        score += countBits(y2) * centerAttack;
    }

    x = pos.Bits(NB);
    while (x)
    {
        f = PopLSB(x);

        int dist = distance(f, pos.King(WHITE));
        score -= knightKingDistance[dist];

        mobility = y = BB_KNIGHT_ATTACKS[f];
        m_pieceAttacks[BlackAttacks] |= m_pieceAttacks[NB] |= y;

        if (y & kingZone[WHITE])
            attackers[BLACK]++;

        mobility &= ~pos.BitsAll(); // exclude enemy/friendly occupied squares
        mobility &= ~m_pieceAttacks[PW]; // exclude attacks by enemy pawns
        score -= knightMobility[countBits(mobility)];

        y &= (pos.Bits(RW) | pos.Bits(QW));
        score -= countBits(y) * strongAttack;

        U64 y2 = y & BB_CENTER[BLACK];
        score -= countBits(y2) * centerAttack;
    }

    return score;
}

/*static */Pair Evaluator::evaluateBishops(Position& pos, U64 occ, U64 kingZone[], int attackers[])
{
    Pair score;
    U64 x, y, mobility;
    FLD f;

    x = pos.Bits(BW);
    while (x)
    {
        f = PopLSB(x);

        int dist = distance(f, pos.King(BLACK));
        score += bishopKingDistance[dist];

        mobility = y = BishopAttacks(f, occ);
        m_pieceAttacks[WhiteAttacks] |= m_pieceAttacks[BW] |= y;

        if (y & kingZone[BLACK])
            attackers[WHITE]++;

        mobility &= ~pos.BitsAll(); // exclude enemy/friendly occupied squares
        mobility &= ~m_pieceAttacks[PB]; // exclude attacks by enemy pawns
        score += bishopMobility[countBits(mobility)];

        y &= (pos.Bits(RB) | pos.Bits(QB));
        score += countBits(y) * strongAttack;

        U64 y2 = y & BB_CENTER[WHITE];
        score += countBits(y2) * centerAttack;
    }

    x = pos.Bits(BB);
    while (x)
    {
        f = PopLSB(x);

        int dist = distance(f, pos.King(WHITE));
        score -= bishopKingDistance[dist];

        mobility = y = BishopAttacks(f, occ);
        m_pieceAttacks[BlackAttacks] |= m_pieceAttacks[BB] |= y;

        if (y & kingZone[WHITE])
            attackers[BLACK]++;

        mobility &= ~pos.BitsAll(); // exclude enemy/friendly occupied squares
        mobility &= ~m_pieceAttacks[PW]; // exclude attacks by enemy pawns
        score -= bishopMobility[countBits(mobility)];

        y &= (pos.Bits(RW) | pos.Bits(QW));
        score -= countBits(y) * strongAttack;

        U64 y2 = y & BB_CENTER[BLACK];
        score -= countBits(y2) * centerAttack;
    }

    return score;
}

/*static */Pair Evaluator::evaluateRooks(Position& pos, U64 occ, U64 kingZone[], int attackers[], PawnHashEntry* ps)
{
    Pair score;
    U64 x, y, mobility;
    FLD f;

    x = pos.Bits(RW);
    while (x)
    {
        f = PopLSB(x);

        int dist = distance(f, pos.King(BLACK));
        score += rookKingDistance[dist];

        mobility = y = RookAttacks(f, occ);
        m_pieceAttacks[WhiteAttacks] |= m_pieceAttacks[RW] |= y;

        if (y & kingZone[BLACK])
            attackers[WHITE]++;

        if (y & pos.Bits(RW))
            score += rooksConnected;

        mobility &= ~pos.BitsAll(); // exclude enemy/friendly occupied squares
        mobility &= ~(m_pieceAttacks[PB] | m_pieceAttacks[NB] | m_pieceAttacks[BB]); // exclude attacks by enemy pieces: pawns, knights and bishops
        score += rookMobility[countBits(mobility)];

        y &= pos.Bits(QB);
        score += countBits(y) * strongAttack;

        U64 y2 = y & BB_CENTER[WHITE];
        score += countBits(y2) * centerAttack;

        int file = Col(f) + 1;
        if (ps->m_ranks[file][WHITE] == 0)
            score += rookOnOpenFile;

        if (Row(f) == 1)
        {
            if ((pos.Bits(PB) & LL(0x00ff000000000000)) || Row(pos.King(BLACK)) == 0)
                score += rookOn7thRank;
        }
    }

    x = pos.Bits(RB);
    while (x)
    {
        f = PopLSB(x);

        int dist = distance(f, pos.King(WHITE));
        score -= rookKingDistance[dist];

        mobility = y = RookAttacks(f, occ);
        m_pieceAttacks[BlackAttacks] |= m_pieceAttacks[RB] |= y;

        mobility &= ~pos.BitsAll(); // exclude enemy/friendly occupied squares
        mobility &= ~(m_pieceAttacks[PW] | m_pieceAttacks[NW] | m_pieceAttacks[BW]); // exclude attacks by enemy pieces: pawns, knights and bishops
        score -= rookMobility[countBits(mobility)];

        if (y & kingZone[WHITE])
            attackers[BLACK]++;

        if (y & pos.Bits(RB))
            score -= rooksConnected;

        y &= pos.Bits(QW);
        score -= countBits(y) * strongAttack;

        U64 y2 = y & BB_CENTER[BLACK];
        score -= countBits(y2) * centerAttack;

        int file = Col(f) + 1;
        if (ps->m_ranks[file][BLACK] == 7)
            score -= rookOnOpenFile;

        if (Row(f) == 6)
        {
            if ((pos.Bits(PW) & LL(0x000000000000ff00)) || Row(pos.King(WHITE)) == 7)
                score -= rookOn7thRank;
        }
    }

    return score;
}

/*static */Pair Evaluator::evaluateQueens(Position& pos, U64 occ, U64 kingZone[], int attackers[])
{
    Pair score;
    U64 x, y, mobility;
    FLD f;

    x = pos.Bits(QW);
    while (x)
    {
        f = PopLSB(x);
        int dist = distance(f, pos.King(BLACK));
        score += queenKingDistance[dist];

        mobility = y = QueenAttacks(f, occ);
        m_pieceAttacks[WhiteAttacks] |= m_pieceAttacks[QW] |= y;

        if (y & kingZone[BLACK])
            attackers[WHITE]++;

        mobility &= ~pos.BitsAll(); // exclude enemy/friendly occupied squares
        mobility &= ~(m_pieceAttacks[PB] | m_pieceAttacks[NB] | m_pieceAttacks[BB] | m_pieceAttacks[RB]); // exclude attacks by enemy pieces: pawns, knights, bishops and rooks
        score += queenMobility[countBits(mobility)];

        U64 y2 = y & BB_CENTER[WHITE];
        score += countBits(y2) * centerAttack;

        if (Row(f) == 1)
        {
            if ((pos.Bits(PB) & LL(0x00ff000000000000)) || Row(pos.King(BLACK)) == 0)
                score += queenOn7thRank;
        }
    }

    x = pos.Bits(QB);
    while (x)
    {
        f = PopLSB(x);
        int dist = distance(f, pos.King(WHITE));
        score -= queenKingDistance[dist];

        mobility = y = QueenAttacks(f, occ);
        m_pieceAttacks[BlackAttacks] |= m_pieceAttacks[QB] |= y;

        if (y & kingZone[WHITE])
            attackers[BLACK]++;

        mobility &= ~pos.BitsAll(); // exclude enemy/friendly occupied squares
        mobility &= ~(m_pieceAttacks[PW] | m_pieceAttacks[NW] | m_pieceAttacks[BW] | m_pieceAttacks[RW]); // exclude attacks by enemy pieces: pawns, knights, bishops and rooks
        score -= queenMobility[countBits(mobility)];

        U64 y2 = y & BB_CENTER[BLACK];
        score -= countBits(y2) * centerAttack;

        if (Row(f) == 6)
        {
            if ((pos.Bits(PW) & LL(0x000000000000ff00)) || Row(pos.King(WHITE)) == 7)
                score -= queenOn7thRank;
        }
    }

    return score;
}

/*static */Pair Evaluator::evaluateKings(Position & pos, U64 occ, PawnHashEntry *ps)
{
    Pair score;
    FLD f;

    f = pos.King(WHITE);
    int fileK = Col(f) + 1;
    int shield = pawnShieldPenalty(ps, fileK, WHITE);
    score += kingPawnShield[shield];
    int storm = pawnStormPenalty(ps, fileK, WHITE);
    score += kingPawnStorm[storm];

    U64 y = QueenAttacks(f, occ);
    score += kingExplosed[countBits(y)];

    f = pos.King(BLACK);
    fileK = Col(f) + 1;
    shield = pawnShieldPenalty(ps, fileK, BLACK);
    score -= kingPawnShield[shield];
    storm = pawnStormPenalty(ps, fileK, BLACK);
    score -= kingPawnStorm[storm];

    y = QueenAttacks(f, occ);
    score -= kingExplosed[countBits(y)];

    return score;
}

/*static */Pair Evaluator::evaluatePiecePairs(Position & pos)
{
    Pair score;

    // piece pairs
    if (pos.Count(NW) >= 2)
        score += knightsPair;
    if (pos.Count(NB) >= 2)
        score -= knightsPair;

    if (pos.Count(BW) >= 2)
        score += bishopsPair;
    if (pos.Count(BB) >= 2)
        score -= bishopsPair;

    if (pos.Count(RW) >= 2)
        score += rooksPair;
    if (pos.Count(RB) >= 2)
        score -= rooksPair;

    // piece combinations
    if (pos.Count(NW) && pos.Count(QW))
        score += knightAndQueen;
    if (pos.Count(NB) && pos.Count(QB))
        score -= knightAndQueen;

    if (pos.Count(BW) && pos.Count(RW))
        score += bishopAndRook;
    if (pos.Count(BB) && pos.Count(RB))
        score -= bishopAndRook;

    return score;
}

/*static */Pair Evaluator::evaluateKingAttackers(Position & pos, int attackers[])
{
    Pair score;

    if (attackers[WHITE] > 3)
        attackers[WHITE] = 3;

    if (attackers[BLACK] > 3)
        attackers[BLACK] = 3;

    score += attackKing[attackers[WHITE]];
    score -= attackKing[attackers[BLACK]];

    if (pos.Side() == WHITE)
        score += tempoAttack;
    else
        score -= tempoAttack;

    return score;
}

int Evaluator::distance(FLD f1, FLD f2)
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

int Evaluator::pawnShieldPenalty(const PawnHashEntry * pEntry, int fileK, COLOR side)
{
    static const int delta[2][8] =
    {
        { 3, 3, 3, 3, 2, 1, 0, 3 },
        { 3, 0, 1, 2, 3, 3, 3, 3 }
    };

    int penalty = 0;
    for (int file = fileK - 1; file < fileK + 2; ++file)
    {
        int rank = pEntry->m_ranks[file][side];
        penalty += delta[side][rank];
    }

    if (penalty < 0)
        penalty = 0;
    if (penalty > 9)
        penalty = 9;

    return penalty;
}

int Evaluator::pawnStormPenalty(const PawnHashEntry * pEntry, int fileK, COLOR side)
{
    COLOR opp = side ^ 1;

    static const int delta[2][8] =
    {
        { 0, 0, 0, 1, 2, 3, 0, 0 },
        { 0, 0, 3, 2, 1, 0, 0, 0 }
    };

    int penalty = 0;
    for (int file = fileK - 1; file < fileK + 2; ++file)
    {
        int rank = pEntry->m_ranks[file][opp];
        penalty += delta[side][rank];
    }

    if (penalty < 0)
        penalty = 0;
    if (penalty > 9)
        penalty = 9;

    return penalty;
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

void Evaluator::showPsq(const char * stable, Pair* table, EVAL mid_w/* = 0*/, EVAL end_w/* = 0*/)
{
    double sum_mid = 0, sum_end = 0;
    if (table != NULL)
    {
        cout << endl << "Midgame: " << stable << endl << endl;
        for (FLD f = 0; f < 64; ++f)
        {
            cout << setw(4) << (table[f].mid - mid_w);
            sum_mid += table[f].mid;

            if (f < 63)
                cout << ", ";
            if (Col(f) == 7)
                cout << endl;
        }

        cout << endl << "Endgame: " << stable << endl << endl;
        for (FLD f = 0; f < 64; ++f)
        {
            cout << setw(4) << (table[f].end - end_w);
            sum_end += table[f].end;

            if (f < 63)
                cout << ", ";
            if (Col(f) == 7)
                cout << endl;
        }
        cout << endl;
        cout << "avg_mid = " << sum_mid / 64. << ", avg_end = " << sum_end / 64. << endl;
        cout << endl;
    }
}

int refParam(int tag, int f)
{
    int * ptr = &(W[lines[tag].start]);
    return ptr[f];
}

void Evaluator::initEval(const vector<int> & x)
{
    W = x;

    for (FLD f = 0; f < 64; ++f) {
        if (Row(f) != 0 && Row(f) != 7)
        {
            PSQ[PW][f].mid = VAL_P + refParam(Mid_Pawn, f);
            PSQ[PW][f].end = VAL_P + refParam(End_Pawn, f);

            passedPawn[f].mid = refParam(Mid_PawnPassed, f);
            passedPawn[f].end = refParam(End_PawnPassed, f);

            passedPawnBlocked[f].mid = refParam(Mid_PawnPassedBlocked, f);
            passedPawnBlocked[f].end = refParam(End_PawnPassedBlocked, f);

            passedPawnFree[f].mid = refParam(Mid_PawnPassedFree, f);
            passedPawnFree[f].end = refParam(End_PawnPassedFree, f);

            passedPawnConnected[f].mid = refParam(Mid_PawnConnectedFree, f);
            passedPawnConnected[f].end = refParam(End_PawnConnectedFree, f);

            pawnDoubled[f].mid = refParam(Mid_PawnDoubled, f);
            pawnDoubled[f].end = refParam(End_PawnDoubled, f);

            pawnIsolated[f].mid = refParam(Mid_PawnIsolated, f);
            pawnIsolated[f].end = refParam(End_PawnIsolated, f);

            pawnDoubledIsolated[f].mid = refParam(Mid_PawnDoubledIsolated, f);
            pawnDoubledIsolated[f].end = refParam(End_PawnDoubledIsolated, f);

            pawnBlocked[f].mid = refParam(Mid_PawnBlocked, f);
            pawnBlocked[f].end = refParam(End_PawnBlocked, f);

            pawnFence[f].mid = refParam(Mid_PawnFence, f);
            pawnFence[f].end = refParam(End_PawnFence, f);
        }
        else
        {
            PSQ[PW][f]              = VAL_P;
            passedPawn[f]           = 0;
            passedPawnBlocked[f]    = 0;
            passedPawnFree[f]       = 0;
            passedPawnConnected[f]  = 0;
            pawnDoubled[f]          = 0;
            pawnIsolated[f]         = 0;
            pawnDoubledIsolated[f]  = 0;
            pawnBlocked[f]          = 0;
            pawnFence[f]            = 0;
        }

        PSQ[NW][f].mid = VAL_N + refParam(Mid_Knight, f);
        PSQ[NW][f].end = VAL_N + refParam(End_Knight, f);

        PSQ[BW][f].mid = VAL_B + refParam(Mid_Bishop, f);
        PSQ[BW][f].end = VAL_B + refParam(End_Bishop, f);

        PSQ[RW][f].mid = VAL_R + refParam(Mid_Rook, f);
        PSQ[RW][f].end = VAL_R + refParam(End_Rook, f);

        PSQ[QW][f].mid = VAL_Q + refParam(Mid_Queen, f);
        PSQ[QW][f].end = VAL_Q + refParam(End_Queen, f);

        PSQ[KW][f].mid = VAL_K + refParam(Mid_King, f);
        PSQ[KW][f].end = VAL_K + refParam(End_King, f);

        PSQ[PB][FLIP[BLACK][f]] = -PSQ[PW][f];
        PSQ[NB][FLIP[BLACK][f]] = -PSQ[NW][f];
        PSQ[BB][FLIP[BLACK][f]] = -PSQ[BW][f];
        PSQ[RB][FLIP[BLACK][f]] = -PSQ[RW][f];
        PSQ[QB][FLIP[BLACK][f]] = -PSQ[QW][f];
        PSQ[KB][FLIP[BLACK][f]] = -PSQ[KW][f];

        knightStrong[f].mid = refParam(Mid_KnightStrong, f);
        knightStrong[f].end = refParam(End_KnightStrong, f);

        bishopStrong[f].mid = refParam(Mid_BishopStrong, f);
        bishopStrong[f].end = refParam(End_BishopStrong, f);

        knightForepost[f].mid = refParam(Mid_KnightForpost, f);
        knightForepost[f].end = refParam(End_KnightForpost, f);
    }

    for (int m = 0; m < 9; ++m) {
        knightMobility[m].mid = refParam(Mid_KnightMobility, m);
        knightMobility[m].end = refParam(End_KnightMobility, m);
    }

    for (int m = 0; m < 14; ++m) {
        bishopMobility[m].mid = refParam(Mid_BishopMobility, m);
        bishopMobility[m].end = refParam(End_BishopMobility, m);
    }

    for (int m = 0; m < 15; ++m) {
        rookMobility[m].mid = refParam(Mid_RookMobility, m);
        rookMobility[m].end = refParam(End_RookMobility, m);
    }

    for (int m = 0; m < 28; ++m) {
        queenMobility[m].mid = refParam(Mid_QueenMobility, m);
        queenMobility[m].end = refParam(End_QueenMobility, m);
    }

    rookOnOpenFile.mid = refParam(Mid_RookOpen, 0);
    rookOnOpenFile.end = refParam(End_RookOpen, 0);

    rookOn7thRank.mid = refParam(Mid_Rook7th, 0);
    rookOn7thRank.end = refParam(End_Rook7th, 0);

    queenOn7thRank.mid = refParam(Mid_Queen7th, 0);
    queenOn7thRank.end = refParam(End_Queen7th, 0);

    for (int d = 0; d < 10; ++d) {
        queenKingDistance[d].mid = refParam(Mid_QueenKingDist, d);
        queenKingDistance[d].end = refParam(End_QueenKingDist, d);

        knightKingDistance[d].mid = refParam(Mid_KnightKingDist, d);
        knightKingDistance[d].end = refParam(End_KnightKingDist, d);

        bishopKingDistance[d].mid = refParam(Mid_BishopKingDist, d);
        bishopKingDistance[d].end = refParam(End_BishopKingDist, d);

        rookKingDistance[d].mid = refParam(Mid_RookKingDist, d);
        rookKingDistance[d].end = refParam(End_RookKingDist, d);

        kingPasserDistance[d].mid = refParam(Mid_KingPassedDist, d);
        kingPasserDistance[d].end = refParam(End_KingPassedDist, d);
    }

    for (int p = 0; p < 10; ++p) {
        kingPawnShield[p].mid = refParam(Mid_KingPawnShield, p);
        kingPawnShield[p].end = refParam(End_KingPawnShield, p);

        kingPawnStorm[p].mid = refParam(Mid_KingPawnStorm, p);
        kingPawnStorm[p].end = refParam(End_KingPawnStorm, p);
    }

    strongAttack.mid = refParam(Mid_AttackStronger, 0);
    strongAttack.end = refParam(End_AttackStronger, 0);

    centerAttack.mid = refParam(Mid_AttackCenter, 0);
    centerAttack.end = refParam(End_AttackCenter, 0);

    tempoAttack.mid = refParam(Mid_Tempo, 0);
    tempoAttack.end = refParam(End_Tempo, 0);

    rooksConnected.mid = refParam(Mid_ConnectedRooks, 0);
    rooksConnected.end = refParam(End_ConnectedRooks, 0);

    bishopsPair.mid = refParam(Mid_BishopsPair, 0);
    bishopsPair.end = refParam(End_BishopsPair, 0);

    rooksPair.mid = refParam(Mid_RooksPair, 0);
    rooksPair.end = refParam(End_RooksPair, 0);

    knightsPair.mid = refParam(Mid_KnightsPair, 0);
    knightsPair.end = refParam(End_KnightsPair, 0);

    pawnOnBiColor.mid = refParam(Mid_PawnOnBiColor, 0);
    pawnOnBiColor.end = refParam(End_PawnOnBiColor, 0);

    knightAndQueen.mid = refParam(Mid_KnightAndQueen, 0);
    knightAndQueen.end = refParam(End_KnightAndQueen, 0);

    bishopAndRook.mid = refParam(Mid_BishopAndRook, 0);
    bishopAndRook.end = refParam(End_BishopAndRook, 0);

    for (int exposed = 0; exposed < 28; ++exposed) {
        kingExplosed[exposed].mid = refParam(Mid_KingExposed, exposed);
        kingExplosed[exposed].end = refParam(End_KingExposed, exposed);
    }

    for (int att = 0; att < 4; ++att) {
        attackKing[att].mid = refParam(Mid_AttackKingZone, att);
        attackKing[att].end = refParam(End_AttackKingZone, att);
    }
}

void Evaluator::initEval()
{
    InitParamLines();

    if (!ReadParamsFromFile(W, "igel.txt"))
    SetDefaultValues(W);
    initEval(W);
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
