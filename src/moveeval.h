/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2019-2023 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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
    static FORCE_INLINE bool isTacticalMove(const Move & mv) { return (mv.Captured() || mv.Promotion()); }
    static bool isSpecialMove(const Move & mv, Search * pSearch);
    static FORCE_INLINE bool isGoodCapture(const Move & mv) { return SORT_VALUE[mv.Captured()] >= SORT_VALUE[mv.Piece()]; }
    static void sortMoves(Search * pSearch, MoveList & mvlist, Move hashMove, int ply);
    static Move getNextBest(MoveList & mvlist, size_t i);
    static EVAL SEE_Exchange(Search * pSearch, FLD to, COLOR side, EVAL currScore, EVAL target, U64 occ);
    static EVAL SEE(Search * pSearch, const Move & mv);

    //
    // sortMoves already ran SEE for every non-promotion capture and encoded the result
    // in the sort score: losing captures score s_SortBadCapture + see, winning ones stay
    // in the capture band. Reuse that instead of a second SEE call. Not valid for the
    // hash move, which keeps s_SortHash whatever it is.
    //

    static FORCE_INLINE bool seeCached(const Move & mv, int sortScore) { return mv.Captured() && !mv.Promotion() && sortScore != s_SortHash; }
    static FORCE_INLINE bool cachedSeeNegative(int sortScore) { return sortScore < s_SortKiller; }
    static FORCE_INLINE EVAL cachedSee(int sortScore) { return sortScore < s_SortKiller ? sortScore - s_SortBadCapture : 0; } // exact when losing, lower bound 0 when winning

    static constexpr EVAL SORT_VALUE[14] = { 0, 0, VAL_P, VAL_P, VAL_N, VAL_N, VAL_B, VAL_B, VAL_R, VAL_R, VAL_Q, VAL_Q, VAL_K, VAL_K };

private:
    static constexpr int s_SortHash       = 7000000;
    static constexpr int s_SortCapture    = 6000000;
    static constexpr int s_SortKiller     = 5000000;
    static constexpr int s_SortCounter    = 4000000;
    static constexpr int s_SortBadCapture = 1000000;
};

#endif // MOVEVAL_H
