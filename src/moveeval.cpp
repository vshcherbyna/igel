/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2019-2020 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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
#include "evaluate.h"

const EVAL SORT_VALUE[14] = { 0, 0, VAL_P, VAL_P, VAL_N, VAL_N, VAL_B, VAL_B, VAL_R, VAL_R, VAL_Q, VAL_Q, VAL_K, VAL_K };

/*static*/ bool MoveEval::isTacticalMove(const Move & mv)
{
    return (mv.Captured() || mv.Promotion());
}

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
        }
        else if (mv == pSearch->m_killerMoves[ply][0] || mv == pSearch->m_killerMoves[ply][1])
            mvlist[j].m_score = s_SortKiller;
        else {
            mvlist[j].m_score = 100 * pSearch->m_history[pSearch->m_position.Side()][mv.From()][mv.To()];

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

    auto mvSize = mvlist.Size();
    for (size_t j = i + 1; j < mvSize; ++j)
    {
        if (mvlist[j].m_score > mvlist[i].m_score)
            std::swap(mvlist[i], mvlist[j]);
    }
    return mvlist[i].m_mv;
}

/*static */bool MoveEval::isGoodCapture(const Move & mv)
{
    return SORT_VALUE[mv.Captured()] >= SORT_VALUE[mv.Piece()];
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
