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

#include "eval_params.h"
#include "notation.h"
#include "utils.h"

vector<int> evalWeights;
int NUM_PARAMS = 0;

Line lines[NUM_LINES] =
{
    Line("Mid_Pawn", 64),
    Line("End_Pawn", 64),
    Line("Mid_Knight", 64),
    Line("End_Knight", 64),
    Line("Mid_Bishop", 64),
    Line("End_Bishop", 64),
    Line("Mid_Rook", 64),
    Line("End_Rook", 64),
    Line("Mid_Queen", 64),
    Line("End_Queen", 64),
    Line("Mid_King", 64),
    Line("End_King", 64),
    Line("Mid_PawnPassed", 7),
    Line("End_PawnPassed", 7),
    Line("Mid_PawnConnected", 64),
    Line("End_PawnConnected", 64),
    Line("Mid_PawnDoubled", 1),
    Line("End_PawnDoubled", 1),
    Line("Mid_PawnIsolated", 1),
    Line("End_PawnIsolated", 1),
    Line("Mid_PawnBlocked", 1),
    Line("End_PawnBlocked", 1),
    Line("Mid_PawnOnBiColor", 1),
    Line("End_PawnOnBiColor", 1),
    Line("Mid_KnightStrong", 64),
    Line("End_KnightStrong", 64),
    Line("Mid_KnightForpost", 64),
    Line("End_KnightForpost", 64),
    Line("Mid_KnightKingDist", 10),
    Line("End_KnightKingDist", 10),
    Line("Mid_BishopStrong", 64),
    Line("End_BishopStrong", 64),
    Line("Mid_BishopMobility", 14),
    Line("End_BishopMobility", 14),
    Line("Mid_KnightMobility", 9),
    Line("End_KnightMobility", 9),
    Line("Mid_BishopKingDist", 10),
    Line("End_BishopKingDist", 10),
    Line("Mid_RookMobility", 15),
    Line("End_RookMobility", 15),
    Line("Mid_QueenMobility", 28),
    Line("End_QueenMobility", 28),
    Line("Mid_RookOpen", 1),
    Line("End_RookOpen", 1),
    Line("Mid_Rook7th", 1),
    Line("End_Rook7th", 1),
    Line("Mid_RookKingDist", 10),
    Line("End_RookKingDist", 10),
    Line("Mid_Queen7th", 1),
    Line("End_Queen7th", 1),
    Line("Mid_QueenKingDist", 10),
    Line("End_QueenKingDist", 10),
    Line("Mid_KingPawnShield", 10),
    Line("End_KingPawnShield", 10),
    Line("Mid_KingPawnStorm", 10),
    Line("End_KingPawnStorm", 10),
    Line("Mid_KingPassedDist", 10),
    Line("End_KingPassedDist", 10),
    Line("Mid_AttackKingZone", 6),
    Line("End_AttackKingZone", 6),
    Line("Mid_AttackStronger", 1),
    Line("End_AttackStronger", 1),
    Line("Mid_AttackCenter", 1),
    Line("End_AttackCenter", 1),
    Line("Mid_ConnectedRooks", 1),
    Line("End_ConnectedRooks", 1),
    Line("Mid_BishopsPair", 1),
    Line("End_BishopsPair", 1),
    Line("Mid_Tempo", 1),
    Line("End_Tempo", 1)
};

void InitParamLines()
{
    for (int i = 1; i < NUM_LINES; ++i)
        lines[i].start = lines[i - 1].start + lines[i - 1].len;
    NUM_PARAMS = lines[NUM_LINES - 1].start + lines[NUM_LINES - 1].len;
}

void SetMaterialOnlyValues(vector<int>& x)
{
    x.resize(NUM_PARAMS);
}

void SetDefaultValues(vector<int>& x)
{
    x.resize(NUM_PARAMS);

    //static const int data[2610] = {26, 0, 0, 0, 0, 0, 0, 0, 45, 38, 33, 37, 36, 45, 39, 44, 27, 21, 19, 6, 24, 29, 25, 21, -3, -3, -6, 18, 12, 0, -5, -7, -19, -21, -7, 7, 11, 0, -17, -25, -13, -17, -5, -13, -2, -2, 10, -10, -21, -10, -21, -16, -16, 10, 17, -17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 60, 59, 53, 58, 54, 62, 62, 47, 51, 41, 35, 27, 35, 42, 40, 15, 16, 12, 11, 0, 3, 12, 10, -9, 1, -12, -11, -12, -13, -4, -11, -19, -8, -16, -11, -7, -12, -15, -22, -8, -9, 1, -3, 9, -7, -12, -24, 0, 0, 0, 0, 0, 0, 0, 0, -62, -48, -41, -31, 12, -59, -36, -62, -57, -47, 30, -10, 1, 14, -3, -35, -40, 19, -17, 11, 26, 52, 24, 14, 0, 13, -2, 26, 12, 38, 7, 9, -3, 12, 7, 5, 15, 11, 22, -5, -10, 8, 20, 25, 35, 26, 34, -6, -7, -17, 7, 21, 21, 28, 13, 4, -40, -1, -15, 2, 23, 4, 2, -2, -62, -39, -13, -25, -21, -38, -54, -62, -20, -1, -21, 7, -6, -16, -20, -39, -20, -15, 20, 14, 6, 3, -18, -33, -9, 8, 25, 23, 23, 17, 3, -15, -5, -8, 19, 23, 21, 16, 10, -10, -10, 0, 2, 14, 7, 4, -15, -12, -26, -19, -6, -8, -3, -18, -16, -33, -24, -24, -17, -6, -17, -13, -29, -47, -31, -25, -62, -47, -40, -42, -15, 2, -43, -16, -42, -46, -11, 25, -17, -62, -25, 8, 13, 0, -1, 26, 11, -9, -15, 3, -3, 26, 16, 16, 9, -15, -2, 2, 6, 21, 26, 8, 4, 9, 0, 13, 19, 11, 17, 32, 17, 5, 10, 22, 15, 6, 8, 24, 29, 2, -17, 10, 3, 1, 12, -6, -18, -15, -20, -18, -15, -10, -3, -7, -19, -22, -3, -3, 4, -10, 1, -5, 2, -7, -6, -7, -3, 3, 2, 0, -4, 3, -1, -2, 10, 15, 18, 8, -7, -1, -16, -4, 9, 13, 6, 7, -7, -11, -12, -9, 4, 5, 11, -1, -2, -16, -24, -21, -14, -5, -1, -12, -15, -32, -23, -16, -15, -11, -14, -11, -13, -24, 18, 22, 0, 22, 23, 4, 13, 15, 6, 7, 18, 18, 25, 25, 0, 9, -4, 14, 13, 16, 0, 19, 33, 6, -18, -15, 4, 19, 6, 22, -8, -14, -26, -20, -1, -3, 7, -6, 8, -21, -28, -18, -3, -10, 5, 7, 3, -24, -29, -9, -12, 2, 8, 14, 4, -49, -4, -3, 8, 16, 19, 18, -19, -4, 24, 19, 24, 19, 20, 22, 18, 19, 13, 13, 12, 10, 1, 12, 18, 11, 19, 17, 17, 17, 16, 12, 11, 8, 19, 18, 24, 12, 17, 18, 14, 17, 18, 19, 15, 13, 8, 10, 6, 9, 7, 10, 3, 8, 0, 2, 5, -1, 4, 2, 6, 6, -2, -3, -4, 9, -5, 0, 1, -2, -6, -8, 1, -19, -16, 11, 7, 1, 21, 22, 23, 25, -10, -19, -5, 4, -42, 20, 20, 40, -11, -10, -9, -34, 1, 13, 8, 12, -26, -27, -31, -28, -20, -10, -17, -21, -8, -29, -13, -19, -9, -12, -10, -11, -2, 8, -6, -7, -5, -1, 7, 5, -16, 1, 13, 8, 17, 17, 0, 16, 13, 7, 13, 22, 7, -3, -2, -33, -4, 14, 21, -2, -1, 11, -3, 21, 3, 10, 15, -1, 10, 2, 18, 12, -4, -4, -14, 19, 6, -4, 5, 5, 22, 25, 1, 3, 6, 4, 36, 25, 5, 28, 3, 22, -1, 0, 16, 16, 14, -11, 17, -12, -7, 13, 16, 15, 0, -4, -4, -11, -12, -6, -20, -16, 0, -10, 3, -20, -2, -5, -8, -23, -51, 35, 39, 27, -3, -3, 22, 24, 36, 32, 27, 37, 20, 24, 10, -26, 28, 34, 38, 20, 31, 45, 49, 11, 10, 19, 16, 10, 4, 16, 10, -23, -28, 21, 10, -9, -6, -1, -20, -34, -1, 10, 3, -6, -6, -1, 5, -11, 2, 21, 4, -26, -13, -4, 17, 14, -28, 22, 15, -52, 2, -31, 18, -4, -60, -30, -17, -16, -3, 15, 8, -14, -11, 15, 15, 16, 17, 31, 17, 11, 1, 12, 16, 14, 13, 37, 32, 6, -17, 10, 17, 20, 18, 22, 19, -1, -15, -8, 14, 19, 21, 17, 10, -6, -12, 1, 12, 19, 21, 19, 14, -5, -18, -6, 11, 17, 18, 14, 5, -11, -40, -20, -9, 8, -12, 6, -15, -31, 0, 0, 0, 0, 0, 0, 0, 0, 51, 37, 32, 41, 39, 46, 44, 44, 24, 10, 5, 10, 9, 22, 8, 6, 3, 1, 6, 1, 13, 26, 8, -4, -2, -9, -10, -10, -5, 2, 8, 11, 0, -8, -8, -11, -1, 14, 11, 20, -1, 6, 7, -16, 0, 0, 13, -8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 61, 60, 58, 51, 55, 49, 62, 62, 47, 43, 39, 30, 28, 33, 42, 42, 31, 32, 32, 21, 29, 29, 37, 33, 22, 22, 17, 22, 19, 13, 29, 16, -1, 10, 4, 8, -6, -11, -10, -12, -12, -12, -21, -10, -28, -29, -22, -20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -8, 33, 5, 4, 29, 39, 4, -47, -14, -2, 11, 3, -6, 38, 5, -12, 14, -12, -1, -11, 9, 5, 32, 11, -4, -21, -19, -5, 10, 24, 8, 3, -10, 4, 12, -9, 15, 35, 13, 17, -2, 3, -6, -8, 23, 11, 27, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -14, -17, -29, -30, -30, -41, -45, -31, 1, -5, -14, -21, -13, -11, -1, 1, 11, 14, 2, 16, 7, 7, 17, 5, 1, 8, 14, 5, 0, 2, -10, -1, 1, 1, 1, -7, -5, -12, 7, 3, -14, -15, -7, -10, -22, -7, -29, -5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 54, 49, 42, 47, 41, 48, 44, 60, 27, 23, 8, 16, 14, 27, 25, 23, -13, 5, -9, -5, -10, 2, 7, -7, -25, -8, -14, -8, -2, -17, -7, 3, -11, -17, -10, -22, -5, -7, -8, -11, -1, 13, 10, -2, -8, 21, -2, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 59, 58, 53, 62, 58, 62, 62, 46, 59, 41, 44, 40, 40, 57, 57, 30, 26, 15, 8, 8, 14, 28, 27, 17, 11, 9, 0, -1, 5, -4, 10, 11, -3, 2, -13, -8, -3, 14, 9, 8, 3, -3, -6, 17, 2, 2, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 57, 35, 28, 40, 57, 33, 50, 58, 23, 42, 26, 41, 46, 40, 57, 46, 17, 28, 13, 12, 27, 3, 16, 35, 11, -5, -26, -9, 11, 15, 16, -2, 1, 1, 4, 41, 11, -7, 5, -28, -8, 5, -8, -4, 2, 27, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 34, 37, 23, 62, 54, 34, 61, 21, 43, 31, 51, 22, 48, 33, 52, 10, 12, 20, 13, 15, 18, -1, 15, -11, -4, 9, -2, 14, 15, 16, 2, -6, 10, -13, 27, 14, -1, -8, -8, -10, -5, -3, 13, -8, 1, 3, -17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -31, 21, -26, 16, 22, 35, -5, 7, -15, -37, -11, 14, 10, 23, 39, -37, -7, 17, -3, -9, -7, -4, 24, -18, -9, 17, -14, -5, 1, 2, 13, -2, -19, -15, -11, 14, -27, -4, -1, -12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 12, -4, -3, 9, -15, 2, 32, -11, 9, 0, -9, -6, -23, -13, -22, -17, -12, -3, -2, -2, -2, -18, -13, -19, -11, -5, -17, -2, -11, -16, -12, -9, -4, -27, -9, -34, -10, -14, -20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 21, 26, 21, 23, 6, 26, 11, 30, -9, -9, -11, 5, 10, 4, 9, -13, -2, -16, -6, -20, -24, -3, 4, -1, -2, -7, -8, -24, -21, -14, -7, -6, -9, -5, -22, -22, -19, -13, -26, -16, -8, -3, 11, -16, -12, -3, -16, -16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 36, 36, 26, 20, 20, 37, 40, 44, 5, -6, 12, 1, 9, 3, -5, 7, -4, -19, -15, -11, -9, -16, -18, -10, -2, -14, -11, -16, -14, -9, -18, -6, -1, -15, -15, -12, -15, -8, -6, -3, 2, -6, -20, -11, -15, -8, 1, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -28, -11, -13, -18, -14, -15, -44, 20, -5, -14, -10, 17, -29, 4, -7, -3, -18, -15, -13, -6, -15, 9, 1, -9, -13, -17, -7, -11, -3, -5, -1, -5, -16, -7, 0, -2, 32, -8, 8, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -14, -4, -23, -9, 2, -27, -22, 40, -11, -20, -15, -4, -27, -14, -31, -3, -15, -1, -17, -15, -2, -16, -9, -8, -17, -7, 0, 2, -6, -3, -6, -15, -15, -17, 15, -22, 16, -3, -5, -9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -15, 24, 4, 7, 17, 41, -10, -40, -11, -7, -3, -14, 16, 11, -17, -15, -8, -5, -5, -10, -8, -8, -19, -8, 2, -6, -8, -6, -13, -5, -3, 12, -3, -1, -8, -6, -3, -9, 1, 2, -8, -2, -12, -32, -22, -7, -6, -8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -10, -23, -23, -22, -31, -42, -43, -32, -12, -17, -10, -15, -10, -18, -21, -12, -9, -10, -15, -14, -12, -13, -11, -12, -1, -9, -15, -12, -9, -14, -4, -4, -1, -4, -6, 2, -7, -2, -8, -9, -5, -6, 0, 3, -9, -9, -11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -15, -4, -13, -12, -30, 2, 7, -3, -5, -2, 0, -8, -1, -3, -3, -8, 0, 0, 0, 0, 0, 0, 0, 0, 4, 7, 5, 5, 4, 6, 5, 6, 12, 4, 10, 17, 28, -3, 2, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -18, -17, -18, -21, -21, -16, -11, -9, -7, -3, -6, -11, -5, 4, 2, -3, 0, 0, 0, 0, 0, 0, 0, 0, 2, 5, 8, 10, 6, 3, 3, 2, 10, 18, 19, 15, 15, 18, 11, 11, 0, 0, 0, 0, 0, 0, 0, 0, -5, -7, 43, 3, 39, 26, 22, 47, 31, 3, -42, 25, -10, 9, 12, 41, 0, 29, 55, 41, 34, 22, 42, 37, 28, 42, 16, 21, 26, 34, 29, 39, 56, 37, 25, 18, 27, 31, 19, 45, 18, 22, 5, 13, 15, -8, 12, 2, -22, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 40, -42, 60, 61, 7, 51, 20, 56, -17, -14, -15, 1, 42, -13, 38, 52, 22, 11, 28, 23, 37, 26, 31, 24, 5, -5, 22, 25, 23, 22, 21, 12, 8, 20, 17, 23, 7, 6, -5, 13, -33, -5, -3, 19, 9, -1, 12, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 40, -1, 0, 0, 0, 0, 0, 0, -7, -5, 12, 12, 3, -23, -28, 45, -32, 14, 8, 19, 57, 57, -42, 20, 2, -27, -41, -3, 31, -3, 29, -11, 24, 19, -12, 31, -25, 25, 39, 30, 50, -41, -19, 29, 16, 39, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -24, 0, 0, 0, 0, 0, 0, 26, -45, -15, -24, -34, -18, -7, -24, 26, 12, 8, -3, 45, 24, 6, -54, 15, 2, 41, 19, 17, -20, -33, -15, 22, -31, 9, 12, 0, -17, 53, -32, -13, 10, 24, 33, 5, 24, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -4, 32, 15, 18, 5, 2, -9, -6, -47, 0, -27, -18, -8, -4, -12, -15, -10, -19, -5, 12, 7, 9, 27, 21, 26, 46, 27, 38, 3, 42, 37, 46, 51, 20, 42, 13, 37, 37, 53, -1, 29, 35, 26, 41, -25, 17, 22, 9, 32, 41, 48, 36, -9, 11, 24, 33, 23, 44, 10, 11, 44, 42, 21, 27, 31, 16, 8, 23, -13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -31, 32, -12, -18, 40, 29, 48, -61, -1, 30, 29, 26, 5, 32, 16, -27, -18, -12, 17, -16, 15, 28, 14, 16, -40, 7, 14, 1, 1, 0, 19, -6, -7, 7, 6, 17, 5, 7, 15, -7, -4, 22, 17, 21, 8, 14, 4, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -20, -5, 5, 10, 14, 16, 17, 16, 18, 17, 31, 29, 16, 27, -29, -23, -14, -5, -4, 0, 1, 1, 1, -4, -6, -9, 5, -10, -22, -6, 3, 5, 12, 15, 18, 23, 31, -51, -22, -18, -10, -8, -3, -11, -12, -27, 0, 4, 18, 4, 3, 7, 11, 2, 3, 3, 0, -33, -15, -10, -4, -6, -10, 0, -3, -4, 18, 5, -22, -16, -11, -7, -8, -2, 2, 10, 12, 20, 22, 27, 26, 28, 26, -32, -20, -9, -2, 9, 10, 11, 11, 14, 14, 14, 14, 15, 13, 10, -22, -17, -14, -11, -5, -4, -2, -1, 4, 6, 10, 13, 16, 20, 24, 33, 27, 31, 34, 28, 31, 34, 32, 32, 18, 13, 48, 20, -48, -48, -35, -25, -23, -18, -15, -8, -9, 2, 5, 8, 8, 10, 15, 20, 18, 16, 30, 32, 31, 40, 33, 37, 23, 23, 55, 16, 27, 7, 11, 17, 0, 26, 20, 12, 7, 1, -4, -5, -11, -12, 0, -26, -1, 2, 2, 3, 4, 4, 17, 14, -23, 14, 0, 63, 41, 4, -5, -6, -9, -22, -25, -38, 0, 63, 41, 27, 16, -2, -25, -23, -21, -33, 31, 26, 14, 11, 3, -4, -10, -23, -20, -29, -32, 1, -4, 0, 4, -2, 5, 8, 12, 4, 7, 6, -4, -18, -29, -30, -48, -25, -18, -7, -7, 0, 6, 15, 15, 9, -5, 2, -38, -63, -23, 3, 15, 10, 11, 5, -1, -10, 5, -8, -34, -27, -14, 4, 15, 21, 23, 23, 14, 4, -27, -23, 26, 61, -3, -8, -14, 37, 58, 26, 0, 9, 3, 7, 24, 18, -19, 3, 27, -5, 26, 21};

    //memcpy(&(x[0]), data, 2610 * sizeof(int));
    //assert(NUM_PARAMS == 2610);
}

void WriteParamsToFile(const vector<int>& x, const std::string& filename)
{
    ofstream ofs(filename.c_str());
    if (!ofs.good())
        return;

    for (int i = 0; i < NUM_LINES; ++i)
    {
        const Line& line = lines[i];
        ofs << line.name;
        for (int j = line.start; j < line.start + line.len; ++j)
            ofs << " " << x[j];
        ofs << endl;
    }
}

std::string ParamNumberToName(size_t n)
{
    stringstream ss;
    ss << "Param_" << n;
    return ss.str();
}

bool ReadParamsFromFile(vector<int>& x, const std::string& filename)
{
    x.resize(NUM_PARAMS);

    ifstream ifs(filename.c_str());
    if (!ifs.good())
        return false;

    string s;
    while (getline(ifs, s))
    {
        vector<string> tokens;
        Split(s, tokens, " ");
        if (tokens.size() < 2)
            continue;

        for (int i = 0; i < NUM_LINES; ++i)
        {
            const Line& line = lines[i];
            if (tokens[0] == line.name)
            {
                for (int j = 0; j < line.len; ++j)
                    x[line.start + j] = atoi(tokens[1 + j].c_str());
            }
        }
    }

#ifdef _DEBUG
    cout << "static const int data[" << NUM_PARAMS << "] = {" << endl;
    for (size_t i = 0; i < NUM_PARAMS; ++i){
        cout << x[i] << ", ";
    }
    cout << "};" << endl;
#endif
    return true;
}
