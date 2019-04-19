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
    Line("Mid_BishopKingDist", 2),
    Line("Mid_BishopAndRook", 1),
    Line("Mid_RookMobility", 3),
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
    Line("End_BishopKingDist", 2),
    Line("End_BishopAndRook", 1),
    Line("End_RookMobility", 3),
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

    static const int data[304] = { -4, -42, 7, 44, -38, 40, -40, -7, -41, -40, 13, 65, -49, 51, -46, -44, -18, -1, -20, 8, 5, 0, -11, 28, 3, 10, -2, -53, 19, 7, 27, 10, 14, 4, 8, -24, 15, 43, 5, 27, 6, -18, -43, 1, -13, 29, 10, 13, -4, 21, 47, -33, 22, -12, 18, 5, 15, 43, -42, -8, 36, 0, -16, -10, -13, -22, -12, 14, 22, 8, 25, -20, -46, 12, -28, 19, -43, 10, -16, 17, 19, 28, -3, -1, -9, 28, 49, -25, -5, -9, -48, 40, -44, -40, 4, 0, 38, 12, -1, -68, 15, -19, -9, 17, -40, -23, 7, -3, 33, -34, 20, 28, -56, 36, 34, 19, -6, 9, 11, 45, 0, 5, 1, -3, -8, -64, 15, -50, -8, -2, -65, -11, 10, -26, -3, 16, -4, 42, -28, -29, 44, 0, 23, 58, -13, 28, 29, -3, -22, 7, -45, 35, 0, 14, 86, 24, 76, 12, -20, -14, -65, -2, 9, -21, -2, 12, 4, 7, 2, -11, -21, 24, -4, 9, 15, 47, 11, 16, -4, 37, -20, 30, -29, -32, -40, 0, -1, -1, 2, 30, 1, 73, 22, 11, 33, -39, -30, -32, -7, 4, 20, -23, 50, 72, 8, 28, -9, -30, 42, -2, 29, 19, -31, -19, -22, 9, 11, 18, -8, 11, 32, 33, -53, -18, -12, 50, 53, 23, -59, -43, 28, -29, -9, -1, -1, 1, -29, -25, 66, -13, 2, 56, -3, -8, -5, -6, 3, 18, 23, -6, 30, -13, -16, 5, 8, -4, -13, 6, -24, 21, 30, -16, -33, 10, -49, 47, -6, 3, 8, 8, -40, 32, 3, 15, 23, 23, 16, 19, -14, -79, -8, -17, 19, -1, -26, -9, -38, -16, -17, -17, -51, 36, 5, -17, -22, -6, 54, 59, 5, 5, 27, -11, 5, 29};

    memcpy(&(x[0]), data, 304 * sizeof(int));
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
