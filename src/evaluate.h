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

#include <string>
#include "eval_params.h"

class Position;
struct PawnHashEntry;

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
    static void initEval(const std::vector<int> & x);
    EVAL evaluate(Position & pos);

private:
    inline Pair evaluatePawns(Position & pos, U64 occ, PawnHashEntry ** pps);
    inline Pair evaluatePawnsAttacks(Position & pos);
    inline Pair evaluateKnights(Position & pos, U64 kingZone[], int attackers[], PawnHashEntry * ps);
    inline Pair evaluateBishops(Position & pos, U64 occ, U64 kingZone[], int attackers[], PawnHashEntry * ps);
    inline Pair evaluateRooks(Position & pos, U64 occ, U64 kingZone[], int attackers[], PawnHashEntry * ps);
    inline Pair evaluateQueens(Position & pos, U64 occ, U64 kingZone[], int attackers[]);
    inline Pair evaluateKings(Position & pos, U64 occ, PawnHashEntry * ps, int attackers[]);
    inline Pair evaluateKingsAttackers(Position & pos, int attackers[]);
    inline Pair evaluatePiecesPairs(Position & pos);
    inline Pair evaluateThreats(Position & pos);

private:
    inline Pair evaluatePawn(Position & pos, COLOR side, U64 occ, PawnHashEntry * ps);
    inline Pair evaluatePawnAttacks(Position & pos, COLOR side);
    inline Pair evaluateKnight(Position & pos, COLOR side, U64 kingZone[], int attackers[], PawnHashEntry * ps);
    inline Pair evaluateBishop(Position & pos, COLOR side, U64 occ, U64 kingZone[], int attackers[], PawnHashEntry * ps);
    inline Pair evaluateRook(Position & pos, COLOR side, U64 occ, U64 kingZone[], int attackers[], PawnHashEntry * ps);
    inline Pair evaluateQueen(Position & pos, COLOR side, U64 occ, U64 kingZone[], int attackers[]);
    inline Pair evaluateKing(Position & pos, COLOR side, U64 occ, PawnHashEntry * ps, int attackers[]);
    inline Pair evaluateKingAttackers(Position & pos, COLOR side, int attackers[]);
    inline Pair evaluatePiecePairs(Position & pos, COLOR side);
    inline Pair evaluateThreat(Position & pos, COLOR side);

private:
    int distance(FLD f1, FLD f2);
    int pawnShieldPenalty(const PawnHashEntry *ps, int fileK, COLOR side);
    int pawnStormPenalty(const PawnHashEntry *ps, int fileK, COLOR side);
    void showPsq(const char * stable, Pair* table, EVAL mid_w = 0, EVAL end_w = 0);

private:
    static constexpr int WhiteAttacks = WHITE;
    static constexpr int BlackAttacks = BLACK;

    U64 m_pieceAttacks        [KB + 1];
    U64 m_pieceAttacks2       [COLORS];
    U32 m_lesserAttacksOnRooks[COLORS];
    U32 m_lesserAttacksOnQueen[COLORS];
    U32 m_majorAttacksOnMinors[COLORS];
    U32 m_minorAttacksOnMinors[COLORS];
    U32 m_kingAttackersWeight [COLORS];
    U32 m_kingAttackers       [COLORS];

public:
    static constexpr int Tempo = 20;
};

namespace Eval {

    std::string trace(const Position& pos);
    Value evaluate(const Position& pos);

    extern bool useNNUE;
    extern std::string eval_file_loaded;
    void init_NNUE();
    void verify_NNUE();

    namespace NNUE {

        Value evaluate(const Position& pos);
        Value compute_eval(const Position& pos);
        void  update_eval(const Position& pos);
        bool  load_eval_file(const std::string& evalFile);

    } // namespace NNUE

} // namespace Eval

#endif
