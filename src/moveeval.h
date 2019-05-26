/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2019 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

#ifndef MOVEVAL_H
#define MOVEVAL_H

#include "moves.h"
#include "search.h"

class MoveEval
{
    MoveEval() = delete;
    ~MoveEval() = delete;

public:
    static bool isTacticalMove(const Move & mv);
    static bool isGoodCapture(const Move & mv);
    static void sortMoves(Search * pSearch, MoveList & mvlist, Move hashMove, int ply);
    static Move getNextBest(MoveList & mvlist, size_t i);
    static EVAL SEE_Exchange(Search * pSearch, FLD to, COLOR side, EVAL currScore, EVAL target, U64 occ);
    static EVAL SEE(Search * pSearch, const Move & mv);

private:
    static constexpr int s_SortHash = 7000000;
    static constexpr int s_SortCapture = 6000000;
    static constexpr int s_SortKiller = 5000000;
};

#endif // MOVEVAL_H
