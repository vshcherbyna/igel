/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2002-2018 Vladimir Medvedev <vrm@bk.ru> (GreKo author)
*  Copyright (C) 2018-2023 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

#ifndef HCE_H
#define HCE_H

#include "types.h"

class Position;

struct Pair
{
    Pair() : mid(0), end(0) {}
    Pair(int m, int e) : mid(m), end(e) {}
    Pair(const Pair& other) : mid(other.mid), end(other.end) {}

    void operator=(const Pair& other) { mid = other.mid; end = other.end; }
    void operator=(int x) { mid = x; end = x; }

    void operator+=(const Pair& other) { mid += other.mid; end += other.end; }
    void operator+=(int x) { mid += x; end += x; }

    void operator-=(const Pair& other) { mid -= other.mid; end -= other.end; }

    Pair operator-() const { return Pair(-mid, -end); }

    int mid;
    int end;
};

inline Pair operator*(int x, const Pair& p) { return Pair(x * p.mid, x * p.end); }

struct PawnHashEntry
{
    PawnHashEntry() : m_pawnHash(0) {}
    void Read(const Position& pos);

    U32 m_pawnHash;
    I8  m_ranks[10][2];

    U64 m_passedPawns[2];
    U64 m_doubledPawns[2];
    U64 m_isolatedPawns[2];
    U64 m_strongFields[2];
    U64 m_backwardPawns;
};

class Hce
{
public:
    static void init();

    static Pair baseScore(Position& pos);

    static EVAL evaluate(Position& pos, const Pair& base);

    static constexpr int Tempo = 20;

private:
    EVAL run(Position& pos, Pair score);

    Pair evaluatePawns(Position& pos, U64 occ, PawnHashEntry** pps);
    Pair evaluatePawnsAttacks(Position& pos);
    Pair evaluateKnights(Position& pos, U64 kingZone[], int attackers[], PawnHashEntry* ps);
    Pair evaluateBishops(Position& pos, U64 occ, U64 kingZone[], int attackers[], PawnHashEntry* ps);
    Pair evaluateRooks(Position& pos, U64 occ, U64 kingZone[], int attackers[], PawnHashEntry* ps);
    Pair evaluateQueens(Position& pos, U64 occ, U64 kingZone[], int attackers[]);
    Pair evaluateKings(Position& pos, U64 occ, PawnHashEntry* ps, int attackers[]);
    Pair evaluateKingsAttackers(Position& pos, int attackers[]);
    Pair evaluatePiecesPairs(Position& pos);
    Pair evaluateThreats(Position& pos);

    Pair evaluatePawn(Position& pos, COLOR side, U64 occ, PawnHashEntry* ps);
    Pair evaluatePawnAttacks(Position& pos, COLOR side);
    Pair evaluateKnight(Position& pos, COLOR side, U64 kingZone[], int attackers[], PawnHashEntry* ps);
    Pair evaluateBishop(Position& pos, COLOR side, U64 occ, U64 kingZone[], int attackers[], PawnHashEntry* ps);
    Pair evaluateRook(Position& pos, COLOR side, U64 occ, U64 kingZone[], int attackers[], PawnHashEntry* ps);
    Pair evaluateQueen(Position& pos, COLOR side, U64 occ, U64 kingZone[], int attackers[]);
    Pair evaluateKing(Position& pos, COLOR side, U64 occ, PawnHashEntry* ps, int attackers[]);
    Pair evaluateKingAttackers(Position& pos, COLOR side, int attackers[]);
    Pair evaluatePiecePairs(Position& pos, COLOR side);
    Pair evaluateThreat(Position& pos, COLOR side);

    int distance(FLD f1, FLD f2);
    int pawnShieldPenalty(const PawnHashEntry* ps, int fileK, COLOR side);
    int pawnStormPenalty(const PawnHashEntry* ps, int fileK, COLOR side);

private:
    U64 m_pieceAttacks        [KB + 1];
    U64 m_pieceAttacks2       [COLORS];
    U32 m_lesserAttacksOnRooks[COLORS];
    U32 m_lesserAttacksOnQueen[COLORS];
    U32 m_majorAttacksOnMinors[COLORS];
    U32 m_minorAttacksOnMinors[COLORS];
    U32 m_kingAttackersWeight [COLORS];
    U32 m_kingAttackers       [COLORS];
};

#endif // HCE_H
