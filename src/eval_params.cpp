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
    Line("Mid_PawnPassedBlocked", 6),
    Line("Mid_PawnPassedFree", 6),
    Line("Mid_PawnConnectedFree", 6),
    Line("Mid_PawnDoubled", 6),
    Line("Mid_PawnIsolated", 6),
    Line("Mid_KnightStrong", 6),
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
    Line("Mid_ConnectedRooks", 1),
    Line("Mid_BishopsPair", 1),
    Line("Mid_RooksPair", 1),
    Line("Mid_KnightsPair", 1),

    Line("End_Pawn", 6),
    Line("End_Knight", 6),
    Line("End_Bishop", 6),
    Line("End_Rook", 6),
    Line("End_Queen", 6),
    Line("End_King", 6),
    Line("End_PawnPassedBlocked", 6),
    Line("End_PawnPassedFree", 6),
    Line("End_PawnConnectedFree", 6),
    Line("End_PawnDoubled", 6),
    Line("End_PawnIsolated", 6),
    Line("End_KnightStrong", 6),
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
    Line("End_AttackStronger", 1),
    Line("End_ConnectedRooks", 1),
    Line("End_BishopsPair", 1),
    Line("End_RooksPair", 1),
    Line("End_KnightsPair", 1),
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

    static const int data[210] = { 4, -43, 15, 32, -18, 24, -9, -43, -27, -5, 8, 27, -17, 14, -5, -4, 8, 24, -18, 2, 21, 8, 17, 2, -24, 9, 2, -29, -9, -2, 37, -29, -2, 4, -20, -39, 2, 10, 10, 13, -29, 14, -13, 6, 10, 23, -1, 20, -3, -22, 50, 3, -22, 2, 4, 51, -31, 33, -18, -30, -17, 14, 29, 20, 18, -18, 8, 16, -5, -31, 26, 5, -13, 31, 14, -26, -23, 6, -69, 15, 8, 13, 43, -3, 11, 8, -9, 19, -40, 0, -15, -56, 16, 4, 1, 17, -28, -15, 24, 7, 45, 9, 30, -48, 1, -10, 19, 85, 49, 39, -2, -23, -24, -56, 9, 8, -13, -36, 22, -10, 9, -27, 10, 8, -8, 9, 3, 4, 22, 17, 15, 1, 27, -19, 22, -35, 3, -18, -5, 24, 15, -3, 8, 5, 27, 16, 22, 36, -1, -12, 91, 44, 48, -33, 32, -10, 23, 30, 14, 17, -58, 11, -35, 9, 10, 4, -1, 33, 12, 1, -13, 1, -7, -7, 8, 6, 13, -16, 9, -11, 20, -30, 4, -16, 37, -26, -30, 32, 3, 9, 26, 8, -4, -48, -16, -2, -9, -18, -32, 41, -10, 14, 3, 13, 9, 4, -4, 3, 26, -1 };

    memcpy(&(x[0]), data, 210 * sizeof(int));
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
