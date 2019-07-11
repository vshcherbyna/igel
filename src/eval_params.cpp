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

#include "eval_params.h"
#include "notation.h"
#include "utils.h"

vector<int> W;
int NUM_PARAMS = 0;

Line lines[NUM_LINES] =
{
    Line("Mid_Pawn", 6),
    Line("Mid_Knight", 6),
    Line("Mid_Bishop", 6),
    Line("Mid_Rook", 6),
    Line("Mid_Queen", 6),
    Line("Mid_King", 6),
    Line("Mid_PawnPassed", 6),
    Line("Mid_PawnPassedBlocked", 6),
    Line("Mid_PawnPassedFree", 6),
    Line("Mid_PawnConnectedFree", 6),
    Line("Mid_PawnDoubled", 6),
    Line("Mid_PawnIsolated", 6),
    Line("Mid_PawnDoubledIsolated", 6),
    Line("Mid_PawnBlocked", 6),
    Line("Mid_PawnFence", 6),
    Line("Mid_PawnOnBiColor", 1),
    Line("Mid_KnightStrong", 6),
    Line("Mid_KnightForpost", 6),
    Line("Mid_KnightKingDist", 2),
    Line("Mid_KnightAndQueen", 1),
    Line("Mid_BishopStrong", 6),
    Line("Mid_BishopMobility", 3),
    Line("Mid_KnightMobility", 3),
    Line("Mid_BishopKingDist", 2),
    Line("Mid_BishopAndRook", 1),
    Line("Mid_RookMobility", 3),
    Line("Mid_QueenMobility", 3),
    Line("Mid_RookOpen", 1),
    Line("Mid_Rook7th", 1),
    Line("Mid_RookKingDist", 2),
    Line("Mid_Queen7th", 1),
    Line("Mid_QueenKingDist", 3),
    Line("Mid_KingPawnShield", 3),
    Line("Mid_KingPawnStorm", 3),
    Line("Mid_KingPassedDist", 3),
    Line("Mid_KingExposed", 3),
    Line("Mid_AttackKing", 4),
    Line("Mid_AttackStronger", 1),
    Line("Mid_AttackCenter", 1),
    Line("Mid_ConnectedRooks", 1),
    Line("Mid_BishopsPair", 1),
    Line("Mid_RooksPair", 1),
    Line("Mid_KnightsPair", 1),
    Line("Mid_Tempo", 1),

    Line("End_Pawn", 6),
    Line("End_Knight", 6),
    Line("End_Bishop", 6),
    Line("End_Rook", 6),
    Line("End_Queen", 6),
    Line("End_King", 6),
    Line("End_PawnPassed", 6),
    Line("End_PawnPassedBlocked", 6),
    Line("End_PawnPassedFree", 6),
    Line("End_PawnConnectedFree", 6),
    Line("End_PawnDoubled", 6),
    Line("End_PawnIsolated", 6),
    Line("End_PawnDoubledIsolated", 6),
    Line("End_PawnBlocked", 6),
    Line("End_PawnFence", 6),
    Line("End_PawnOnBiColor", 1),
    Line("End_KnightStrong", 6),
    Line("End_KnightForpost", 6),
    Line("End_KnightKingDist", 2),
    Line("End_KnightAndQueen", 1),
    Line("End_BishopStrong", 6),
    Line("End_BishopMobility", 3),
    Line("End_KnightMobility", 3),
    Line("End_BishopKingDist", 2),
    Line("End_BishopAndRook", 1),
    Line("End_RookMobility", 3),
    Line("End_QueenMobility", 3),
    Line("End_RookOpen", 1),
    Line("End_Rook7th", 1),
    Line("End_RookKingDist", 2),
    Line("End_Queen7th", 1),
    Line("End_QueenKingDist", 3),
    Line("End_KingPawnShield", 3),
    Line("End_KingPawnStorm", 3),
    Line("End_KingExposed", 3),
    Line("End_KingPassedDist", 3),
    Line("End_AttackKing", 4),
    Line("End_AttackStronger", 1),
    Line("End_AttackCenter", 1),
    Line("End_ConnectedRooks", 1),
    Line("End_BishopsPair", 1),
    Line("End_RooksPair", 1),
    Line("End_KnightsPair", 1),
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

    static const int data[316] = {-7, -12, 11, 26, -25, 4, -8, -46, -22, 3, -12, 65, 15, -20, -33, -25, -18, 67, -23, -26, 24, 20, -28, -21, 45, -34, 8, -47, 8, 56, -6, 18, 31, 17, -12, -20, 1, 16, -40, 13, 9, 21, 42, -17, 45, 17, -20, -15, 4, -13, 39, 1, 14, -25, -7, 6, -21, 4, -14, -5, -36, 52, -2, 44, 19, -5, 19, -11, 6, 8, 8, -28, -32, 10, -33, -18, -3, 22, -1, -6, 22, 8, 15, -1, -24, -13, 2, 3, -28, 17, -31, -20, 15, -31, -32, 10, 9, 0, 12, -18, -18, 5, -22, 16, -9, 55, 3, 12, 25, -17, -5, 26, -37, 16, 39, -25, 28, 36, -7, -9, 11, -12, 28, 43, -3, 33, 54, -20, -27, 2, -15, 23, 26, -50, 2, -15, -67, 47, 10, -46, 9, -16, -22, 21, -23, -67, -16, -36, -1, 11, -3, 29, 6, 60, -33, -38, 24, 8, 17, -26, 71, 40, 39, 23, -25, 19, -36, -8, 7, 11, -18, -11, -9, 7, 8, 8, -22, 31, -7, 12, 13, 68, 12, -11, -3, 37, -11, 52, -20, -18, -43, -8, 20, -23, 10, 22, 25, 73, 0, 5, -19, 1, -38, 38, -5, 30, -14, 26, 40, 65, 24, 23, -27, 33, 35, -1, 36, 2, -16, 5, 20, -20, 0, -10, -13, 14, 44, 21, -12, -14, -39, 14, 10, -3, -19, -12, 17, -4, -49, -33, -17, -20, 11, -50, -22, -18, 35, 10, -6, -21, 7, 3, -12, 10, 28, 12, 15, -11, 2, 6, 40, -26, -35, 10, 19, -12, -23, -19, 17, 12, -28, 9, 11, -16, -3, -7, 8, 7, 18, -20, 11, 27, -2, 35, 23, 20, 12, 18, 18, 20, 43, -33, 34, -3, 14, -13, -14, 31, 32, -2, -28, -20, -25, 48, -32, -10, -11, 6, 68, 55, 2, 4, 33, 13, 11, 27};
    memcpy(&(x[0]), data, 316 * sizeof(int));

    assert(NUM_PARAMS == 316);
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
