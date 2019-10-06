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

#ifndef EVAL_PARAMS_H
#define EVAL_PARAMS_H

#include "types.h"

extern vector<int> evalWeights;

struct Line
{
    Line() : name(""), start(0), len(0) {}
    Line(const string& n, int l) : name(n), start(0), len(l) {}

    string name;
    int start;
    int len;
};

enum Param
{
    Mid_Pawn,
    Mid_Knight,
    Mid_Bishop,
    Mid_Rook,
    Mid_Queen,
    Mid_King,
    Mid_PawnPassed,
    Mid_PawnPassedBlocked,
    Mid_PawnPassedFree,
    Mid_PawnConnectedFree,
    Mid_PawnDoubled,
    Mid_PawnIsolated,
    Mid_PawnDoubledIsolated,
    Mid_PawnBlocked,
    Mid_PawnFence,
    Mid_PawnOnBiColor,
    Mid_KnightStrong,
    Mid_KnightForpost,
    Mid_KnightKingDist,
    Mid_KnightAndQueen,
    Mid_BishopStrong,
    Mid_BishopMobility,
    Mid_KnightMobility,
    Mid_BishopKingDist,
    Mid_BishopAndRook,
    Mid_RookMobility,
    Mid_QueenMobility,
    Mid_RookOpen,
    Mid_Rook7th,
    Mid_RookKingDist,
    Mid_Queen7th,
    Mid_QueenKingDist,
    Mid_KingPawnShield,
    Mid_KingPawnStorm,
    Mid_KingPassedDist,
    Mid_AttackKingZone,
    Mid_AttackStronger,
    Mid_AttackCenter,
    Mid_ConnectedRooks,
    Mid_BishopsPair,
    Mid_RooksPair,
    Mid_KnightsPair,
    Mid_Tempo,

    End_Pawn,
    End_Knight,
    End_Bishop,
    End_Rook,
    End_Queen,
    End_King,
    End_PawnPassed,
    End_PawnPassedBlocked,
    End_PawnPassedFree,
    End_PawnConnectedFree,
    End_PawnDoubled,
    End_PawnIsolated,
    End_PawnDoubledIsolated,
    End_PawnBlocked,
    End_PawnFence,
    End_PawnOnBiColor,
    End_KnightStrong,
    End_KnightForpost,
    End_KnightKingDist,
    End_KnightAndQueen,
    End_BishopStrong,
    End_BishopMobility,
    End_KnightMobility,
    End_BishopKingDist,
    End_BishopAndRook,
    End_RookMobility,
    End_QueenMobility,
    End_RookOpen,
    End_Rook7th,
    End_RookKingDist,
    End_Queen7th,
    End_QueenKingDist,
    End_KingPawnShield,
    End_KingPawnStorm,
    End_KingPassedDist,
    End_AttackKingZone,
    End_AttackStronger,
    End_AttackCenter,
    End_ConnectedRooks,
    End_BishopsPair,
    End_RooksPair,
    End_KnightsPair,
    End_Tempo,

    NUM_LINES
};

extern Line lines[NUM_LINES];
extern int NUM_PARAMS;

void InitParamLines();

void SetDefaultValues(vector<int>& x);
void SetMaterialOnlyValues(vector<int>& x);
void WriteParamsToFile(const vector<int>& x, const std::string& filename);
std::string ParamNumberToName(size_t n);
bool ReadParamsFromFile(vector<int>& x, const std::string& filename);

#endif
