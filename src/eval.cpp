/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2002-2018 Vladimir Medvedev <vrm@bk.ru> (GreKo author)
*  Copyright (C) 2018-2020 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

Pair pieceSquareTables[14][64];

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
Pair attackKingZone[8];
Pair strongAttack;
Pair centerAttack;
Pair tempoAttack;
Pair queenSafeChecksPenalty;
Pair rookSafeChecksPenalty;
Pair bishopSafeChecksPenalty;
Pair knightSafeChecksPenalty;

//
//  Piece combinations evaluations
//

Pair rooksPair;
Pair bishopsPair;
Pair knightsPair;
Pair knightAndQueen;
Pair bishopAndRook;

EVAL Evaluator::evaluate(Position & pos)
{
    U64 kingZone[2]     = { BB_KING_ATTACKS[pos.King(WHITE)], BB_KING_ATTACKS[pos.King(BLACK)] };
    int attackKing[2]   = { 0, 0 };

    PawnHashEntry *ps = nullptr;
    memset(m_pieceAttacks, 0, sizeof(m_pieceAttacks));

    auto occ   = pos.BitsAll();
    auto score = pos.Score();

    score += evaluatePawns(pos, occ, &ps);
    score += evaluatePawnsAttacks(pos);
    score += evaluateKnights(pos, kingZone, attackKing, ps);
    score += evaluateBishops(pos, occ, kingZone, attackKing, ps);
    score += evaluateRooks(pos, occ, kingZone, attackKing, ps);
    score += evaluateQueens(pos, occ, kingZone, attackKing);
    score += evaluateKings(pos, occ, ps);
    score += evaluatePiecesPairs(pos);
    score += evaluateKingsAttackers(pos, attackKing);

    auto mid = pos.MatIndex(WHITE) + pos.MatIndex(BLACK);
    auto end = 64 - mid;

    EVAL eval = (score.mid * mid + score.end * end) / 64;

    if (pos.Side() == BLACK)
        eval = -eval;

    return eval;
}

Pair Evaluator::evaluatePawns(Position & pos, U64 occ, PawnHashEntry ** pps)
{
    size_t index = pos.PawnHash() % pos.m_pawnHashSize;
    PawnHashEntry & ps = pos.m_pawnHashTable[index];

    if (ps.m_pawnHash != pos.PawnHash())
        ps.Read(pos);

    *pps = &ps;

    Pair score{};

    score += evaluatePawn(pos, WHITE, occ, &ps);
    score -= evaluatePawn(pos, BLACK, occ, &ps);

    assert((score.mid >= -2000 && score.mid <= 2000) && (score.end >= -2000 && score.end <= 2000));

    return score;
}

Pair Evaluator::evaluatePawn(Position & pos, COLOR side, U64 occ, PawnHashEntry * ps)
{
    Pair score{};

    auto opp   = side ^ 1;
    auto pawns = pos.Bits(PAWN | side);
    auto x     = ps->m_passedPawns[side];

    m_pieceAttacks[side] = m_pieceAttacks[PAWN | side] = side == WHITE ? (((pawns << 9) & L1MASK) | ((pawns << 7) & R1MASK)) : (((pawns >> 9) & R1MASK) | ((pawns >> 7) & L1MASK));

    while (x) {
        auto f = PopLSB(x);
        score += passedPawn[FLIP[side][f]];

        auto freeFile = [](FLD field, U64 occ, Position & pos, COLOR side) -> bool
        {
            auto rank = Row(field);
            auto end_of_file = side == WHITE ? field - (8 * rank) : field + ((7 - rank) * 8);
            return ((BB_BETWEEN[field][end_of_file] & occ) == 0 && pos[end_of_file] == NOPIECE);
        };

        auto dir = side == WHITE ? DIR_U : DIR_D;

        // check if it is a blocked or a free pass pawn
        if (pos[f - 8 + 16 * side] != NOPIECE)
            score += passedPawnBlocked[FLIP[side][f]];
        else if ((BB_DIR[f][dir] & occ) == 0)
            score += passedPawnFree[FLIP[side][f]];

        // check if it is a connected pass pawn
        if ((BB_PAWN_CONNECTED[f] & ps->m_passedPawns[side]) && freeFile(f, occ, pos, side))
            score += passedPawnConnected[FLIP[side][f]];

        score += kingPasserDistance[distance(f - 8 + 16 * side, pos.King(opp))];
        score -= kingPasserDistance[distance(f - 8 + 16 * side, pos.King(side))];
    }

    x = ps->m_doubledPawns[side];
    while (x)
        score += pawnDoubled[FLIP[side][PopLSB(x)]];

    x = ps->m_isolatedPawns[side];
    while (x)
        score += pawnIsolated[FLIP[side][PopLSB(x)]];

    x = ps->m_doubledPawns[side] & ps->m_isolatedPawns[side];
    while (x)
        score += pawnDoubledIsolated[FLIP[side][PopLSB(x)]];

    auto direction = side == WHITE ? Down(occ) : Up(occ);
    x = pos.Bits(PAWN | side) & direction;
    while (x)
        score += pawnBlocked[FLIP[side][PopLSB(x)]];

    direction = side == WHITE ? Down(pos.Bits(PB)) : Up(pos.Bits(PW));
    x = pos.Bits(PAWN | side) & direction;
    while (x)
        score += pawnFence[FLIP[side][PopLSB(x)]];

    if (pos.Count(BISHOP | side) == 1) {
        U64 mask = (pos.Bits(BISHOP | side) & BB_WHITE_FIELDS) ? BB_WHITE_FIELDS : BB_BLACK_FIELDS;
        score += countBits(pos.Bits(PAWN | side) & mask) * pawnOnBiColor;
    }

    return score;
}

Pair Evaluator::evaluatePawnsAttacks(Position & pos)
{
    Pair score{};

    score += evaluatePawnAttacks(pos, WHITE);
    score -= evaluatePawnAttacks(pos, BLACK);

    assert((score.mid >= -1000 && score.mid <= 1000) && (score.end >= -1000 && score.end <= 1000));

    return score;
}

Pair Evaluator::evaluatePawnAttacks(Position & pos, COLOR side)
{
    Pair score{};

    auto opp = side ^ 1;
    auto x   = pos.Bits(PAWN | side);
    U64  y   = side == WHITE ? UpRight(x) | UpLeft(x) : DownRight(x) | DownLeft(x);

    y &= (pos.Bits(KNIGHT | opp) | pos.Bits(BISHOP | opp) | pos.Bits(ROOK | opp) | pos.Bits(QUEEN | opp));
    score += countBits(y) * strongAttack;

    auto y2 = y & BB_CENTER[side];
    score += countBits(y2) * centerAttack;

    return score;
}

Pair Evaluator::evaluateKnights(Position& pos, U64 kingZone[], int attackers[], PawnHashEntry * ps)
{
    Pair score{};

    score += evaluateKnight(pos, WHITE, kingZone, attackers, ps);
    score -= evaluateKnight(pos, BLACK, kingZone, attackers, ps);

    assert((score.mid >= -1000 && score.mid <= 1000) && (score.end >= -1000 && score.end <= 1000));

    return score;
}

Pair Evaluator::evaluateKnight(Position & pos, COLOR side, U64 kingZone[], int attackers[], PawnHashEntry * ps)
{
    Pair score{};

    auto opp = side ^ 1;
    auto x   = pos.Bits(KNIGHT | side);

    while (x) {
        auto f    = PopLSB(x);
        auto dist = distance(f, pos.King(opp));

        score += knightKingDistance[dist];

        auto mobility = BB_KNIGHT_ATTACKS[f];
        auto y        = mobility;

        m_pieceAttacks[side] |= m_pieceAttacks[KNIGHT | side] |= y;

        if (y & kingZone[opp])
            attackers[side]++;

        mobility &= ~pos.BitsAll(); // exclude enemy/friendly occupied squares
        mobility &= ~m_pieceAttacks[PAWN | opp]; // exclude attacks by enemy pawns
        score += knightMobility[countBits(mobility)];

        y &= (pos.Bits(ROOK | opp) | pos.Bits(QUEEN | opp));
        score += countBits(y) * strongAttack;

        auto y2 = y & BB_CENTER[side];
        score += countBits(y2) * centerAttack;

        if (BB_SINGLE[f] & ps->m_strongFields[side]) {
            score += knightStrong[FLIP[side][f]];
            auto file = Col(f) + 1;
            if (ps->m_ranks[file][side] == 7 * side) {
                auto dir = side == WHITE ? BB_DIR[f][DIR_D] : BB_DIR[f][DIR_U];
                if (dir & pos.Bits(KNIGHT | side))
                    score += knightForepost[FLIP[side][f]];
            }
        }
    }

    return score;
}

Pair Evaluator::evaluateBishops(Position & pos, U64 occ, U64 kingZone[], int attackers[], PawnHashEntry * ps)
{
    Pair score{};

    score += evaluateBishop(pos, WHITE, occ, kingZone, attackers, ps);
    score -= evaluateBishop(pos, BLACK, occ, kingZone, attackers, ps);

    assert((score.mid >= -1000 && score.mid <= 1000) && (score.end >= -1000 && score.end <= 1000));

    return score;
}

Pair Evaluator::evaluateBishop(Position & pos, COLOR side, U64 occ, U64 kingZone[], int attackers[], PawnHashEntry * ps)
{
    Pair score{};

    auto opp = side ^ 1;
    auto x   = pos.Bits(BISHOP | side);

    while (x) {
        auto f    = PopLSB(x);
        auto dist = distance(f, pos.King(opp));

        score += bishopKingDistance[dist];

        auto mobility = BishopAttacks(f, occ);
        auto y        = mobility;

        m_pieceAttacks[side] |= m_pieceAttacks[BISHOP | side] |= y;

        if (y & kingZone[opp])
            attackers[side]++;

        mobility &= ~pos.BitsAll(); // exclude enemy/friendly occupied squares
        mobility &= ~m_pieceAttacks[PAWN | opp]; // exclude attacks by enemy pawns
        score += bishopMobility[countBits(mobility)];

        y &= (pos.Bits(ROOK | opp) | pos.Bits(QUEEN | opp));
        score += countBits(y) * strongAttack;

        auto y2 = y & BB_CENTER[side];
        score += countBits(y2) * centerAttack;

        if (BB_SINGLE[f] & ps->m_strongFields[side])
            score += bishopStrong[FLIP[side][f]];
    }

    return score;
}

Pair Evaluator::evaluateRooks(Position & pos, U64 occ, U64 kingZone[], int attackers[], PawnHashEntry * ps)
{
    Pair score{};

    score += evaluateRook(pos, WHITE, occ, kingZone, attackers, ps);
    score -= evaluateRook(pos, BLACK, occ, kingZone, attackers, ps);
    
    assert((score.mid >= -1000 && score.mid <= 1000) && (score.end >= -1000 && score.end <= 1000));

    return score;
}

Pair Evaluator::evaluateRook(Position & pos, COLOR side, U64 occ, U64 kingZone[], int attackers[], PawnHashEntry* ps)
{
    Pair score{};

    auto opp = side ^ 1;
    auto x   = pos.Bits(ROOK | side);

    while (x) {
        auto f      = PopLSB(x);
        auto dist   = distance(f, pos.King(opp));

        score += rookKingDistance[dist];

        auto mobility = RookAttacks(f, occ);
        auto y        = mobility;

        m_pieceAttacks[side] |= m_pieceAttacks[ROOK | side] |= y;

        if (y & kingZone[opp])
            attackers[side]++;

        if (y & pos.Bits(ROOK | side))
            score += rooksConnected;

        mobility &= ~pos.BitsAll(); // exclude enemy/friendly occupied squares
        mobility &= ~(m_pieceAttacks[PAWN | opp] | m_pieceAttacks[KNIGHT | opp] | m_pieceAttacks[BISHOP | opp]); // exclude attacks by enemy pieces: pawns, knights and bishops
        score += rookMobility[countBits(mobility)];

        y &= pos.Bits(QUEEN | opp);
        score += countBits(y) * strongAttack;

        auto y2 = y & BB_CENTER[side];
        score += countBits(y2) * centerAttack;

        auto file = Col(f) + 1;
        if (ps->m_ranks[file][side] == 7 * side)
            score += rookOnOpenFile;

        if (Row(f) == 1 + 5 * side) { // seventh rank
            U64 ranks[] = { LL(0x000000000000ff00), LL(0x00ff000000000000) };
            if ((pos.Bits(PAWN | opp) & ranks[opp]) || (Row(pos.King(opp)) == 7 * side))
                score += rookOn7thRank;
        }
    }

    return score;
}

Pair Evaluator::evaluateQueens(Position & pos, U64 occ, U64 kingZone[], int attackers[])
{
    Pair score{};

    score += evaluateQueen(pos, WHITE, occ, kingZone, attackers);
    score -= evaluateQueen(pos, BLACK, occ, kingZone, attackers);

    assert((score.mid >= -1000 && score.mid <= 1000) && (score.end >= -1000 && score.end <= 1000));

    return score;
}

Pair Evaluator::evaluateQueen(Position & pos, COLOR side, U64 occ, U64 kingZone[], int attackers[])
{
    Pair score{};

    auto opp = side ^ 1;
    auto x   = pos.Bits(QUEEN | side);

    while (x) {
        auto f    = PopLSB(x);
        auto dist = distance(f, pos.King(opp));

        score += queenKingDistance[dist];

        auto mobility = QueenAttacks(f, occ);
        auto y        = mobility;

        m_pieceAttacks[side] |= m_pieceAttacks[QUEEN | side] |= y;

        if (y & kingZone[opp])
            attackers[side]++;

        mobility &= ~pos.BitsAll(); // exclude enemy/friendly occupied squares
        mobility &= ~(m_pieceAttacks[PAWN | opp] | m_pieceAttacks[KNIGHT | opp] | m_pieceAttacks[BISHOP | opp] | m_pieceAttacks[ROOK | opp]); // exclude attacks by enemy pieces: pawns, knights, bishops and rooks

        score += queenMobility[countBits(mobility)];

        auto y2 = y & BB_CENTER[side];
        score += countBits(y2) * centerAttack;

        if (Row(f) == 1 + 5 * side) { // seventh rank
            U64 ranks[] = {LL(0x000000000000ff00), LL(0x00ff000000000000)};
            if ((pos.Bits(PAWN | opp) & ranks[opp]) || (Row(pos.King(opp)) == 7 * side))
                score += queenOn7thRank;
        }
    }

    return score;
}

Pair Evaluator::evaluateKingsAttackers(Position & pos, int attackers[])
{
    Pair score{};

    score += evaluateKingAttackers(pos, WHITE, attackers);
    score -= evaluateKingAttackers(pos, BLACK, attackers);

    if (pos.Side() == WHITE)
        score += tempoAttack;
    else
        score -= tempoAttack;

    assert((score.mid >= -1000 && score.mid <= 1000) && (score.end >= -1000 && score.end <= 1000));

    return score;
}

Pair Evaluator::evaluateKingAttackers(Position& pos, COLOR side, int attackers[])
{
    Pair score{};

    if (attackers[side] > 3)
        attackers[side] = 3;

    score += attackKingZone[attackers[side]];

    return score;
}

Pair Evaluator::evaluateKings(Position & pos, U64 occ, PawnHashEntry *ps)
{
    Pair score{};

    score += evaluateKing(pos, WHITE, occ, ps);
    score -= evaluateKing(pos, BLACK, occ, ps);

    assert((score.mid >= -1000 && score.mid <= 1000) && (score.end >= -1000 && score.end <= 1000));

    return score;
}

Pair Evaluator::evaluateKing(Position & pos, COLOR side, U64 occ, PawnHashEntry * ps)
{
    Pair score{};

    auto f      = pos.King(side);
    auto fileK  = Col(f) + 1;
    auto shield = pawnShieldPenalty(ps, fileK, side);
    auto storm  = pawnStormPenalty(ps, fileK, side);

    score += kingPawnShield[shield];
    score += kingPawnStorm[storm];

    COLOR opp = side ^ 1;

    m_pieceAttacks[side] |= m_pieceAttacks[KING | side] |= BB_KING_ATTACKS[f];

    auto weak = m_pieceAttacks[opp] & ~m_pieceAttacks[side];
    auto safe = ~pos.BitsAll(opp);
    safe &= ~m_pieceAttacks[side] | (weak & m_pieceAttacks[opp]);

    auto king = pos.Count(ROOK | opp) ? RookAttacks(f, occ) : 0;
    auto rookChecks = safe & m_pieceAttacks[ROOK | opp] & king;

    if (rookChecks)
        score += rookSafeChecksPenalty;

    auto kingq = pos.Count(QUEEN | opp) ? QueenAttacks(f, occ) : 0;
    auto queenChecks = safe & m_pieceAttacks[QUEEN | opp] & kingq & ~rookChecks;

    if (queenChecks)
        score += queenSafeChecksPenalty;

    auto kingb = pos.Count(BISHOP | opp) ? BishopAttacks(f, occ) : 0;
    auto bishopChecks = safe & m_pieceAttacks[BISHOP | opp] & kingb & ~queenChecks;

    if (bishopChecks)
        score += bishopSafeChecksPenalty;

    auto kingk = pos.Count(KNIGHT | opp) ? BB_KNIGHT_ATTACKS[f] : 0;
    auto knightChecks = safe & m_pieceAttacks[KNIGHT | opp] & kingk;

    if (knightChecks)
        score += knightSafeChecksPenalty;

    return score;
}

Pair Evaluator::evaluatePiecesPairs(Position & pos)
{
    Pair score{};

    score += evaluatePiecePairs(pos, WHITE);
    score -= evaluatePiecePairs(pos, BLACK);

    assert((score.mid >= -1000 && score.mid <= 1000) && (score.end >= -1000 && score.end <= 1000));

    return score;
}

Pair Evaluator::evaluatePiecePairs(Position & pos, COLOR side)
{
    Pair score{};

    if (pos.Count(KNIGHT | side) >= 2)
        score += knightsPair;

    if (pos.Count(BISHOP | side) >= 2)
        score += bishopsPair;

    if (pos.Count(ROOK | side) >= 2)
        score += rooksPair;

    if (pos.Count(KNIGHT | side) && pos.Count(QUEEN | side))
        score += knightAndQueen;

    if (pos.Count(BISHOP | side) && pos.Count(ROOK | side))
        score += bishopAndRook;

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
    int * ptr = &(evalWeights[lines[tag].start]);
    return ptr[f];
}

void Evaluator::initEval(const vector<int> & x)
{
    evalWeights = x;

    for (FLD f = 0; f < 64; ++f) {
        if (Row(f) != 0 && Row(f) != 7)
        {
            pieceSquareTables[PW][f].mid = VAL_P + refParam(Mid_Pawn, f);
            pieceSquareTables[PW][f].end = VAL_P + refParam(End_Pawn, f);

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
            pieceSquareTables[PW][f]    = VAL_P;
            passedPawn[f]               = 0;
            passedPawnBlocked[f]        = 0;
            passedPawnFree[f]           = 0;
            passedPawnConnected[f]      = 0;
            pawnDoubled[f]              = 0;
            pawnIsolated[f]             = 0;
            pawnDoubledIsolated[f]      = 0;
            pawnBlocked[f]              = 0;
            pawnFence[f]                = 0;
        }

        pieceSquareTables[NW][f].mid = VAL_N + refParam(Mid_Knight, f);
        pieceSquareTables[NW][f].end = VAL_N + refParam(End_Knight, f);

        pieceSquareTables[BW][f].mid = VAL_B + refParam(Mid_Bishop, f);
        pieceSquareTables[BW][f].end = VAL_B + refParam(End_Bishop, f);

        pieceSquareTables[RW][f].mid = VAL_R + refParam(Mid_Rook, f);
        pieceSquareTables[RW][f].end = VAL_R + refParam(End_Rook, f);

        pieceSquareTables[QW][f].mid = VAL_Q + refParam(Mid_Queen, f);
        pieceSquareTables[QW][f].end = VAL_Q + refParam(End_Queen, f);

        pieceSquareTables[KW][f].mid = VAL_K + refParam(Mid_King, f);
        pieceSquareTables[KW][f].end = VAL_K + refParam(End_King, f);

        pieceSquareTables[PB][FLIP[BLACK][f]] = -pieceSquareTables[PW][f];
        pieceSquareTables[NB][FLIP[BLACK][f]] = -pieceSquareTables[NW][f];
        pieceSquareTables[BB][FLIP[BLACK][f]] = -pieceSquareTables[BW][f];
        pieceSquareTables[RB][FLIP[BLACK][f]] = -pieceSquareTables[RW][f];
        pieceSquareTables[QB][FLIP[BLACK][f]] = -pieceSquareTables[QW][f];
        pieceSquareTables[KB][FLIP[BLACK][f]] = -pieceSquareTables[KW][f];

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

    for (int att = 0; att < 4; ++att) {
        attackKingZone[att].mid = refParam(Mid_AttackKingZone, att);
        attackKingZone[att].end = refParam(End_AttackKingZone, att);
    }

    queenSafeChecksPenalty.mid = refParam(Mid_QueenSafeChecksPenalty, 0);
    queenSafeChecksPenalty.end = refParam(End_QueenSafeChecksPenalty, 0);

    rookSafeChecksPenalty.mid = refParam(Mid_RookSafeChecksPenalty, 0);
    rookSafeChecksPenalty.end = refParam(End_RookSafeChecksPenalty, 0);

    bishopSafeChecksPenalty.mid = refParam(Mid_BishopSafeChecksPenalty, 0);
    bishopSafeChecksPenalty.end = refParam(End_BishopSafeChecksPenalty, 0);

    knightSafeChecksPenalty.mid = refParam(Mid_KnightSafeChecksPenalty, 0);
    knightSafeChecksPenalty.end = refParam(End_KnightSafeChecksPenalty, 0);
}

void Evaluator::initEval()
{
    initParams();
    setDefaultWeights(evalWeights);
    initEval(evalWeights);
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
