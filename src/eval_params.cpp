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

static const char* weightsFile[] = {
    #include "weights.txt"
    ""
};

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
    Line("Mid_PawnPassed", 64),
    Line("End_PawnPassed", 64),
    Line("Mid_PawnPassedBlocked", 64),
    Line("End_PawnPassedBlocked", 64),
    Line("Mid_PawnPassedFree", 64),
    Line("End_PawnPassedFree", 64),
    Line("Mid_PawnConnectedFree", 64),
    Line("End_PawnConnectedFree", 64),
    Line("Mid_PawnDoubled", 64),
    Line("End_PawnDoubled", 64),
    Line("Mid_PawnIsolated", 64),
    Line("End_PawnIsolated", 64),
    Line("Mid_PawnDoubledIsolated", 64),
    Line("End_PawnDoubledIsolated", 64),
    Line("Mid_PawnBlocked", 64),
    Line("End_PawnBlocked", 64),
    Line("Mid_PawnFence", 64),
    Line("End_PawnFence", 64),
    Line("Mid_PawnOnBiColor", 1),
    Line("End_PawnOnBiColor", 1),
    Line("Mid_KnightStrong", 64),
    Line("End_KnightStrong", 64),
    Line("Mid_KnightForpost", 64),
    Line("End_KnightForpost", 64),
    Line("Mid_KnightKingDist", 10),
    Line("End_KnightKingDist", 10),
    Line("Mid_KnightAndQueen", 1),
    Line("End_KnightAndQueen", 1),
    Line("Mid_BishopStrong", 64),
    Line("End_BishopStrong", 64),
    Line("Mid_BishopMobility", 14),
    Line("End_BishopMobility", 14),
    Line("Mid_KnightMobility", 9),
    Line("End_KnightMobility", 9),
    Line("Mid_BishopKingDist", 10),
    Line("End_BishopKingDist", 10),
    Line("Mid_BishopAndRook", 1),
    Line("End_BishopAndRook", 1),
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
    Line("Mid_AttackKingZone", 4),
    Line("End_AttackKingZone", 4),
    Line("Mid_AttackStronger", 1),
    Line("End_AttackStronger", 1),
    Line("Mid_AttackCenter", 1),
    Line("End_AttackCenter", 1),
    Line("Mid_ConnectedRooks", 1),
    Line("End_ConnectedRooks", 1),
    Line("Mid_BishopsPair", 1),
    Line("End_BishopsPair", 1),
    Line("Mid_RooksPair", 1),
    Line("End_RooksPair", 1),
    Line("Mid_KnightsPair", 1),
    Line("End_KnightsPair", 1),
    Line("Mid_Tempo", 1),
    Line("End_Tempo", 1),
    Line("Mid_QueenSafeChecksPenalty", 1),
    Line("End_QueenSafeChecksPenalty", 1),
    Line("Mid_RookSafeChecksPenalty", 1),
    Line("End_RookSafeChecksPenalty", 1),
    Line("Mid_BishopSafeChecksPenalty", 1),
    Line("End_BishopSafeChecksPenalty", 1),
    Line("Mid_KnightSafeChecksPenalty", 1),
    Line("End_KnightSafeChecksPenalty", 1),
};

void initParams()
{
    for (int i = 1; i < NUM_LINES; ++i)
        lines[i].start = lines[i - 1].start + lines[i - 1].len;

    NUM_PARAMS = lines[NUM_LINES - 1].start + lines[NUM_LINES - 1].len;
}

void setDefaultWeights(vector<int>& x)
{
    x.resize(NUM_PARAMS);

    for (auto w = 0; strcmp(weightsFile[w], ""); w++) {
        vector<string> tokens;
        string s = weightsFile[w];
        Split(s, tokens, " ");
        if (tokens.size() < 2)
            continue;

        for (auto i = 0; i < NUM_LINES; ++i) {
            const Line& line = lines[i];
            if (tokens[0] == line.name) {
                for (int j = 0; j < line.len; ++j)
                    x[line.start + j] = atoi(tokens[1 + j].c_str());
            }
        }
    }
}

void writeParams(const vector<int>& x, const std::string& filename)
{
    ofstream ofs(filename.c_str());

    if (!ofs.good())
        return;

    for (int i = 0; i < NUM_LINES; ++i) {
        const Line& line = lines[i];
        ofs << "\"" << line.name;
        for (int j = line.start; j < line.start + line.len; ++j)
            ofs << " " << x[j];
        ofs << "\"," << endl;
    }
}

std::string ParamNumberToName(size_t n)
{
    stringstream ss;
    ss << "Param_" << n;
    return ss.str();
}
