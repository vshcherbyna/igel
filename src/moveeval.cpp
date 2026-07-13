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

#include "moveeval.h"
#include "nnue.h"


/*static*/ bool MoveEval::isSpecialMove(const Move & mv, Search * pSearch)
{
    PIECE piece = mv.Piece();

    //
    // non pawn moves are no special
    //

    if (!(piece == PB || piece == PW))
        return false;

    //
    //  check pawn move rank, and if it is 5th or 6th treat it as a special move
    //

    int row = Row(mv.To());
    auto passerPush = piece == PB ? (row == 5 || row == 6) : (row == 1 || row == 2);

    return passerPush;
}

/*static */void MoveEval::sortMoves(Search * pSearch, MoveList & mvlist, Move hashMove, int ply)
{
    auto counterMove = ply >= 1 ? pSearch->m_moveStack[ply - 1] : Move{};
    auto counterPiece = ply >= 1 ? pSearch->m_pieceStack[ply - 1] : 0;
    auto counterTo = ply >= 1 ? counterMove.To() : 0;

    auto followMove = ply >= 2 ? pSearch->m_moveStack[ply - 2] : Move{};
    auto followPiece = ply >= 2 ? pSearch->m_pieceStack[ply - 2] : 0;
    auto followTo = ply >= 2 ? followMove.To() : 0;

    auto mvSize = mvlist.Size();
    const auto killer0 = pSearch->m_killerMoves[ply][0];
    const auto killer1 = pSearch->m_killerMoves[ply][1];
    const auto side = pSearch->m_position.Side();

    for (size_t j = 0; j < mvSize; ++j)
    {
        Move mv = mvlist[j].m_mv;
        if (mv == hashMove)
        {
            mvlist[j].m_score = s_SortHash;
            mvlist.Swap(j, 0);
        }
        else if (mv.Captured() || mv.Promotion())
        {
            EVAL s_piece = SORT_VALUE[mv.Piece()];
            EVAL s_captured = SORT_VALUE[mv.Captured()];
            EVAL s_promotion = SORT_VALUE[mv.Promotion()];

            mvlist[j].m_score = s_SortCapture + 10 * (s_captured + s_promotion) - s_piece;

            if (mv.Captured() && !mv.Promotion()) { // add SEE score for captures if it's negative (losing captures)
                auto see = MoveEval::SEE(pSearch, mv);
                if (see < 0)
                    mvlist[j].m_score = s_SortBadCapture + see; // penalize bad captures, but still keep them above non-captures
            }
        }
        else if (mv == killer0 || mv == killer1)
            mvlist[j].m_score = s_SortKiller;
        else if (counterMove && mv == pSearch->m_counterTable[counterPiece][counterTo])
            mvlist[j].m_score = s_SortCounter;
        else {
            mvlist[j].m_score = pSearch->m_history[side][mv.From()][mv.To()];

            if (counterMove)
                mvlist[j].m_score += pSearch->m_followTable[0][counterPiece][counterTo][mv.Piece()][mv.To()];

            if (followMove)
                mvlist[j].m_score += pSearch->m_followTable[1][followPiece][followTo][mv.Piece()][mv.To()];
        }
    }
}

/*static */Move MoveEval::getNextBest(MoveList & mvlist, size_t i)
{
    if (i == 0 && mvlist[0].m_score == s_SortHash)
        return mvlist[0].m_mv;

    size_t best_idx = i;
    int best_score = mvlist[i].m_score;
    const size_t size = mvlist.Size();

    // branchless selection: avoid branch misprediction in the hot inner loop
    for (size_t j = i + 1; j < size; ++j) {
        int score_j = mvlist[j].m_score;
        bool better = score_j > best_score;
        best_idx   = better ? j         : best_idx;
        best_score = better ? score_j   : best_score;
    }

    // if we found a better move, do a single swap
    if (best_idx != i)
        std::swap(mvlist[i], mvlist[best_idx]);

    return mvlist[i].m_mv;
}


/*static */EVAL MoveEval::SEE_Exchange(Search * pSearch, FLD to, COLOR side, EVAL currScore, EVAL target, U64 occ)
{
    U64 att = pSearch->m_position.GetAttacks(to, side, occ) & occ;
    if (att == 0)
        return currScore;

    FLD from = NF;
    PIECE piece;
    EVAL newTarget = SORT_VALUE[KW] + 1;

    while (att)
    {
        FLD f = PopLSB(att);
        piece = pSearch->m_position[f];
        if (SORT_VALUE[piece] < newTarget)
        {
            from = f;
            newTarget = SORT_VALUE[piece];
        }
    }

    assert(from < 64);
    occ ^= BB_SINGLE[from];
    EVAL score = -SEE_Exchange(pSearch, to, side ^ 1, -(currScore + target), newTarget, occ);
    return (score > currScore) ? score : currScore;
}

/*static */EVAL MoveEval::SEE(Search * pSearch, const Move & mv)
{
    FLD from = mv.From();
    FLD to = mv.To();
    PIECE piece = mv.Piece();
    PIECE captured = mv.Captured();
    PIECE promotion = mv.Promotion();
    COLOR side = GetColor(piece);

    EVAL score0 = SORT_VALUE[captured];
    if (promotion)
    {
        score0 += SORT_VALUE[promotion] - SORT_VALUE[PW];
        piece = promotion;
    }

    U64 occ = pSearch->m_position.BitsAll() ^ BB_SINGLE[from];
    EVAL score = -SEE_Exchange(pSearch, to, side ^ 1, -score0, SORT_VALUE[piece], occ);

    return score;
}
