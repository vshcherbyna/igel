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
    End_Pawn,
    Mid_Knight,
    End_Knight,
    Mid_Bishop,
    End_Bishop,
    Mid_Rook,
    End_Rook,
    Mid_Queen,
    End_Queen,
    Mid_King,
    End_King,
    Mid_PawnPassed,
    End_PawnPassed,
    Mid_PawnConnected,
    End_PawnConnected,
    Mid_PawnDoubled,
    End_PawnDoubled,
    Mid_PawnIsolated,
    End_PawnIsolated,
    Mid_PawnBlocked,
    End_PawnBlocked,
    Mid_PawnOnBiColor,
    End_PawnOnBiColor,
    Mid_KnightStrong,
    End_KnightStrong,
    Mid_KnightForpost,
    End_KnightForpost,
    Mid_KnightKingDist,
    End_KnightKingDist,
    Mid_BishopStrong,
    End_BishopStrong,
    Mid_BishopMobility,
    End_BishopMobility,
    Mid_KnightMobility,
    End_KnightMobility,
    Mid_BishopKingDist,
    End_BishopKingDist,
    Mid_RookMobility,
    End_RookMobility,
    Mid_QueenMobility,
    End_QueenMobility,
    Mid_RookOpen,
    End_RookOpen,
    Mid_Rook7th,
    End_Rook7th,
    Mid_RookKingDist,
    End_RookKingDist,
    Mid_Queen7th,
    End_Queen7th,
    Mid_QueenKingDist,
    End_QueenKingDist,
    Mid_KingPawnShield,
    End_KingPawnShield,
    Mid_KingPawnStorm,
    End_KingPawnStorm,
    Mid_KingPassedDist,
    End_KingPassedDist,
    Mid_AttackKingZone,
    End_AttackKingZone,
    Mid_AttackStronger,
    End_AttackStronger,
    Mid_AttackCenter,
    End_AttackCenter,
    Mid_ConnectedRooks,
    End_ConnectedRooks,
    Mid_BishopsPair,
    End_BishopsPair,
    Mid_Tempo,
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
