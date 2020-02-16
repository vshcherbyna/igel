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

    static const int data[1602] = {0, 0, 0, 0, 0, 0, 0, 0, 186, 107, 144, 163, 142, 142, 108, 73, -5, -21, -5, -5, 5, 54, 11, -5, -10, -13, -7, -8, 9, 16, -3, -8, -13, -23, -8, 3, 6, 7, -8, -8, -16, -18, -10, -8, 5, -3, 10, -11, -20, -21, -19, -23, -12, -3, 8, -27, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 128, 143, 122, 87, 90, 102, 127, 135, 42, 43, 19, -4, -6, 8, 35, 35, 23, 17, 7, -1, -3, 0, 11, 9, 7, 8, -4, -3, -5, -4, 1, -7, 5, 4, -2, -4, -1, -3, -7, -10, 8, 3, 3, 0, 8, 4, -6, -8, 0, 0, 0, 0, 0, 0, 0, 0, -170, -98, -56, -72, -4, -99, -66, -117, -48, -25, -16, -2, -24, 24, -15, -26, -16, 11, -12, 19, 29, 33, 9, -5, 17, 19, 30, 34, 22, 53, 26, 43, 15, 19, 25, 25, 27, 26, 32, 17, -4, 21, 21, 29, 40, 32, 40, 16, -5, 7, 14, 26, 22, 23, 26, 17, -7, -8, -4, 19, 20, 25, -1, 8, -33, -8, -3, 5, -7, -21, -27, -71, -14, 3, 3, 8, -3, -13, -12, -31, -10, -1, 25, 13, 11, -3, -9, -21, -4, 7, 14, 16, 15, 12, 2, -21, -9, 3, 14, 17, 21, 10, 2, -15, -9, 1, 4, 17, 7, 2, -6, -10, -12, -17, -3, -8, -6, -8, -18, -1, -13, -22, -7, -14, -12, -21, -22, -36, -41, -41, -43, -76, -76, -61, -23, -52, -31, -31, -29, -25, -57, -33, -59, -47, -5, -5, -9, -2, -7, 26, 2, 6, -12, 12, 8, 23, 25, 15, 18, -13, 6, -6, 14, 34, 31, 15, 11, 31, 8, 33, 26, 25, 27, 36, 40, 29, 14, 20, 29, 14, 22, 36, 38, 34, 18, 20, 1, 11, 23, -1, 21, 26, -1, 12, 12, 15, 10, 9, -13, -7, -4, 2, 10, 12, 10, 9, 7, 2, 0, 9, 10, 8, 5, -1, 5, -5, 8, 2, 8, 14, 15, 7, -7, 2, -2, 4, 13, 10, 7, 6, 1, -20, -12, 4, 9, 9, 12, 6, -5, -12, -11, -17, -16, -4, -4, -13, -14, -38, -20, -15, -8, -14, -10, -5, -22, -29, 6, 9, 18, 13, -5, 26, 24, 25, 10, 5, 25, 32, 9, 39, 26, 45, -13, 14, 1, 0, 20, 21, 67, 26, -12, -17, -7, -8, -9, -3, 5, -5, -21, -25, -22, -13, -11, -24, 14, -6, -24, -18, -15, -9, 1, 2, 28, 7, -17, -18, -3, 0, 3, 7, 19, -10, -14, -5, 1, 6, 11, 7, 15, -8, 31, 29, 32, 32, 30, 33, 24, 28, 13, 21, 21, 12, 18, 10, 14, 6, 35, 30, 36, 29, 19, 21, 7, 8, 33, 37, 35, 31, 22, 20, 22, 22, 27, 26, 29, 25, 21, 26, 3, 14, 21, 16, 18, 15, 5, 7, -5, 1, 11, 12, 13, 12, 10, 3, -5, 8, 7, 4, 11, 3, 0, 6, -2, 0, -44, -36, -19, -15, -24, 1, -4, -30, -2, -6, -17, -22, -57, 2, -15, 38, -18, -15, -29, -36, -44, -25, -27, -16, -25, -14, -23, -31, -48, -29, -34, -26, -3, -18, -16, -17, -19, -23, -18, 1, -1, 10, 0, -14, -4, 2, 16, 11, 3, 3, 8, 6, 6, 13, 15, 21, 22, 8, 8, 6, 7, 3, 13, 16, 17, 8, 12, -15, -17, 5, 1, -6, 2, 20, 29, 2, 27, -2, 30, -5, 5, 4, 18, 8, 18, 14, 25, -4, 30, 19, 6, 9, 9, -2, 29, 11, 2, 23, 3, 20, 1, 3, 13, 2, 13, -4, 17, -4, -11, 8, 4, -4, 6, -3, -9, -1, -5, -9, -23, -17, -14, -10, 15, -11, -6, -2, 6, -14, -39, 54, 52, 16, -9, -5, 15, -1, 35, 38, 36, 57, 10, 64, 20, -14, -22, 76, 49, 31, 26, 95, 70, -21, -50, 10, -23, -49, -24, -32, -43, -77, -66, -45, -12, -44, -52, -46, -56, -119, -19, -7, -23, -27, -25, -26, -9, -31, 35, 1, -1, -18, -25, -13, 10, 28, 1, 21, 7, -61, -17, -46, 4, 17, -132, -36, -17, 7, 25, 11, -19, -84, -1, 33, 49, 33, 48, 45, 56, 12, 1, 29, 35, 42, 41, 30, 36, 9, -9, 15, 31, 41, 38, 35, 27, 5, -5, 15, 23, 32, 31, 24, 19, 7, -13, 4, 15, 19, 20, 16, 10, 0, -18, 3, 9, 10, 14, 11, 0, -17, -30, -22, -10, 8, -15, 4, -15, -43, 0, -9, -20, -14, -1, 18, -78, 0, -13, -1, 28, 62, 102, 50, 0, 0, 0, 0, 0, 0, 0, 0, 31, 104, 144, 75, 44, 82, 28, 8, 13, 88, 66, 52, 62, 12, 104, -3, 43, 29, 12, 40, 17, 42, 25, 40, 10, 19, 0, -13, -21, 5, 0, -7, 10, 17, 4, -6, -8, -15, 7, -15, 8, 23, -17, -23, 12, 5, 38, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 40, 11, 4, 30, 57, 73, 12, -19, 22, 31, 28, 28, 18, 25, 3, 5, 8, 17, 2, 6, 17, 4, 18, 8, -4, 9, 2, 1, 18, 11, 18, 12, -12, 6, -3, 1, 5, 6, 9, -3, -15, -2, 4, 3, -19, -18, -7, -8, 0, 0, 0, 0, 0, 0, 0, 0, -6, -18, -13, -7, -7, -12, -2, -7, -1, 47, 15, 19, 21, 29, 10, -23, 23, 40, 19, 69, 77, 44, -33, -37, 34, 12, 42, 36, 32, 55, 44, 52, 32, 26, 28, 42, 49, 45, 60, -5, 20, 30, 23, 29, 37, 38, 34, 35, 2, -1, 7, -14, 12, -5, -15, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 199, -11, 74, 7, 47, 60, -14, -37, 22, 11, -5, 26, -1, 24, 59, -8, 3, 17, 33, 30, 41, 36, 15, 14, 8, 20, 22, 22, 22, 9, 24, -2, 16, 16, 16, 14, 9, 14, 2, -9, 5, 7, 13, 8, -3, 15, -8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 39, 32, -32, 3, -54, 27, 14, -7, 4, -23, -13, 0, 35, 90, -69, 37, -56, -28, 21, 7, -9, 10, 20, -27, -15, 51, -6, -13, -5, -28, 21, -23, -35, -17, 3, -24, -86, 1, 45, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 16, -9, 0, 0, 0, 0, 5, 33, -74, -1, -41, -46, 5, -1, -1, 31, 1, 5, 8, -25, -1, 66, -82, 21, -4, 3, 9, 12, 1, -42, -6, -10, -24, 19, 10, 31, -16, 52, -6, -5, 5, 10, 34, 8, 42, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 25, 33, 12, 22, 11, 13, 3, 9, -27, 0, -33, -24, -8, -5, -7, -12, -6, -12, -1, 9, 13, 14, 38, 37, 56, 3, 7, 52, 39, 58, 35, 111, 51, 42, -33, -4, 38, 27, 42, 50, 35, 23, -40, 18, 33, 20, 32, 47, 43, 39, 11, 14, 35, 36, 42, 53, 36, 34, 3, 22, 4, 24, 19, 19, 3, 19, -30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -183, 29, -8, -55, -2, -86, -52, -192, -31, 11, -14, 6, -4, -36, -5, -61, -41, -11, 0, -1, -5, 13, -10, -17, -21, 8, 5, 1, -6, -5, 10, -12, 1, 10, 1, 6, 1, 3, 12, 0, 0, 16, 8, 18, 14, 3, -5, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -26, -10, -1, 7, 14, 18, 16, 15, 15, 18, 21, 25, 7, 18, -33, -24, -7, -2, -2, 1, 6, 8, 10, 10, 3, 5, 11, 7, -35, -13, -1, 6, 10, 15, 21, 21, 33, -40, -16, -1, 0, 4, 7, 2, 1, -14, 0, 26, 37, 10, 11, 8, 11, 11, 20, 14, 0, -29, -11, -3, 3, 4, 3, 9, 0, 4, -11, -5, -6, -3, -6, -2, 1, 4, 8, 14, 19, 21, 36, 38, 34, -30, -16, -1, 0, 9, 13, 13, 13, 16, 17, 17, 15, 12, 7, 5, -45, -19, -12, -9, -6, -6, -3, 0, 3, 4, 8, 10, 16, 12, 24, 30, 36, 45, 20, 54, 32, 55, 32, 47, 1, 13, 1, 7, -94, -72, -45, -29, -19, -10, -3, 0, 1, 7, 6, 10, 7, 16, 12, 6, 0, 7, 18, 11, 23, 15, 2, 0, 14, 17, 49, 86, 25, 15, -20, 32, 0, 40, 25, 7, -1, -2, -4, -9, -14, -4, 0, -22, -1, 5, 7, 5, 4, 11, 19, 17, -36, 24, 0, 569, 89, 40, 26, 14, 11, 1, -6, -20, 0, 128, 48, 48, 29, 20, 1, 4, 0, 0, 54, 46, 28, 22, 5, -7, -17, -31, -33, -55, -41, -17, -18, -14, -5, -7, 1, 3, 8, 3, 7, 9, -6, -10, -33, -52, -76, -117, -22, -2, -11, -4, 5, 9, 18, 30, 21, 28, 44, -173, -75, 7, 15, 12, 12, 3, -11, -16, -22, -7, -48, -45, -29, -16, -5, 3, 11, 13, 9, -7, -15, -8, 24, 99, 206, 385, 3, 0, 1, -4, 13, -16, 62, 14, 1, 8, 1, 3, 14, 18, 29, 17};

    memcpy(&(x[0]), data, 1602 * sizeof(int));
    assert(NUM_PARAMS == 1602);
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
