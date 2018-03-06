/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2002-2018 Vladimir Medvedev <vrm@bk.ru> (GreKo author)
*  Copyright (C) 2018 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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
    Line("Mid_PawnPassedBlocked", 6),
    Line("Mid_PawnPassedFree", 6),
    Line("Mid_PawnDoubled", 6),
    Line("Mid_PawnIsolated", 6),
    Line("Mid_KnightStrong", 6),
    Line("Mid_BishopPair", 1),
    Line("Mid_BishopStrong", 6),
    Line("Mid_BishopMobility", 3),
    Line("Mid_RookMobility", 3),
    Line("Mid_RookOpen", 1),
    Line("Mid_Rook7th", 1),
    Line("Mid_Queen7th", 1),
    Line("Mid_QueenKingDist", 3),
    Line("Mid_KingPawnShield", 3),
    Line("Mid_KingPassedDist", 3),
    Line("Mid_AttackKing", 4),
    Line("Mid_AttackStronger", 1),

    Line("End_Pawn", 6),
    Line("End_Knight", 6),
    Line("End_Bishop", 6),
    Line("End_Rook", 6),
    Line("End_Queen", 6),
    Line("End_King", 6),
    Line("End_PawnPassedBlocked", 6),
    Line("End_PawnPassedFree", 6),
    Line("End_PawnDoubled", 6),
    Line("End_PawnIsolated", 6),
    Line("End_KnightStrong", 6),
    Line("End_BishopPair", 1),
    Line("End_BishopStrong", 6),
    Line("End_BishopMobility", 3),
    Line("End_RookMobility", 3),
    Line("End_RookOpen", 1),
    Line("End_Rook7th", 1),
    Line("End_Queen7th", 1),
    Line("End_QueenKingDist", 3),
    Line("End_KingPawnShield", 3),
    Line("End_KingPassedDist", 3),
    Line("End_AttackKing", 4),
    Line("End_AttackStronger", 1)
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

    static const int data[192] =
    {
        -39, 9, 18, 62, -13, -13, -9, -2, -7, 10, 32, 4, 18, 0, -9, 2, 4, 13, -1, -25, 18, 25, 5, -43, 2, -23, 8, 12, -13, -6, 23, -3, 20, 22, 0, 6, -4, -1, 32, 24, 22, -4, -1, 27, 44, 42, -11, -6, -7, -12,
        51, 29, -25, -9, 5, 21, -5, -15, 25, -20, -33, -35, -33, 11, -3, 52, 25, 26, -45, 20, -14, -24, 27, -30, 17, -11, 11, 33, -55, 14, 27, -30, 41, -31, -5, -6, -42, -30, -37, -20, 1, -4, 6, 44, 108, 31, -16, 24, 60, 47,
        -2, -6, -14, -43, -46, 48, 2, -13, 9, -44, -5, 31, -31, -8, 19, -3, -5, 33, -1, 49, -9, -10, -38, 40, 21, -65, -12, -3, -19, 23, 52, -26, -11, 26, 20, 35, -13, 13, -10, 32, 12, 72, 8, 33, -1, -3, 30, 0, 2, -9,
        11, -6, -5, -12, 8, -20, 14, 15, 29, -30, 30, 0, 50, -73, 56, -3, -2, -19, 6, -8, 37, -4, -4, 14, -30, 10, -3, 29, 17, -11, -61, 8, -24, 8, -60, 7, -1, 2, -3, -12, -10, 24
    };

    memcpy(&(x[0]), data, 192 * sizeof(int));
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

    return true;
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

