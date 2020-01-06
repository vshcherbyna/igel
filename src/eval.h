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

#ifndef EVAL_H
#define EVAL_H

#include "eval_params.h"
#include "position.h"

const EVAL VAL_P = 100;
const EVAL VAL_N = 310;
const EVAL VAL_B = 330;
const EVAL VAL_R = 500;
const EVAL VAL_Q = 1000;
const EVAL VAL_K = 20000;

class Evaluator
{
public:
    static void initEval();
    static void initEval(const vector<int>& x);
    EVAL evaluate(Position & pos);

private:
    inline Pair evaluatePawns(Position & pos, U64 occ, PawnHashEntry ** pps);
    inline Pair evaluatePawnsAttacks(Position & pos);
    inline Pair evaluateKnights(Position & pos, U64 kingZone[], int attackers[]);
    inline Pair evaluateBishops(Position & pos, U64 occ, U64 kingZone[], int attackers[]);
    inline Pair evaluateOutposts(Position & pos, PawnHashEntry *ps);
    inline Pair evaluateRooks(Position & pos, U64 occ, U64 kingZone[], int attackers[], PawnHashEntry *ps);
    inline Pair evaluateQueens(Position & pos, U64 occ, U64 kingZone[], int attackers[]);
    inline Pair evaluateKings(Position & pos, U64 occ, PawnHashEntry *ps);
    inline Pair evaluateKingAttackers(Position & pos, int attackers[]);

private:
    Pair evaluatePiecePairs(Position & pos);

private:
    int distance(FLD f1, FLD f2);
    int pawnShieldPenalty(const PawnHashEntry *ps, int fileK, COLOR side);
    int pawnStormPenalty(const PawnHashEntry *ps, int fileK, COLOR side);
    void showPsq(const char * stable, Pair* table, EVAL mid_w = 0, EVAL end_w = 0);

private:
    static constexpr int WhiteAttacks = NOPIECE;
    static constexpr int BlackAttacks = NOPIECE + 1;
    U64 m_pieceAttacks[KB];
};

#endif
