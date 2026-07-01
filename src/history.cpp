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

#include "history.h"
#include "search.h"

#include <algorithm>

/*static */constexpr int History::s_historyMax;
/*static */constexpr int History::s_historyMultiplier;
/*static */constexpr int History::s_historyDivisor;

/*static */void History::updateHistory(Search * pSearch, const MoveList & quetMoves, int ply, int bonus)
{
    assert(ply < MAX_PLY);

    if (ply < 2 || !quetMoves.Size())
        return;

    auto counterMove = pSearch->m_moveStack[ply - 1];
    auto counterPiece = pSearch->m_pieceStack[ply - 1];
    auto counterTo = counterMove.To();

    auto followMove = pSearch->m_moveStack[ply - 2];
    auto followPiece = pSearch->m_pieceStack[ply - 2];
    auto followTo = followMove.To();

    auto colour = pSearch->m_position.Side();

    auto s = quetMoves.Size();
    auto best = quetMoves[s - 1].m_mv;

    bonus = std::min(bonus, s_historyMax);

    for (size_t i = 0; i < s; ++i) {
        Move mv = quetMoves[i].m_mv;
        auto delta = (mv == best) ? bonus : -bonus;
        auto from = mv.From();
        auto piece = mv.Piece();
        auto to = mv.To();
        int entry = pSearch->m_history[colour][from][to];
        entry += s_historyMultiplier * delta - entry * abs(delta) / s_historyDivisor;
        pSearch->m_history[colour][from][to] = static_cast<int16_t>(entry);

        if (counterMove) {
            int centry = pSearch->m_followTable[0][counterPiece][counterTo][piece][to];
            centry += s_historyMultiplier * delta - centry * abs(delta) / s_historyDivisor;
            pSearch->m_followTable[0][counterPiece][counterTo][piece][to] = static_cast<int16_t>(centry);
        }

        if (followMove) {
            int fentry = pSearch->m_followTable[1][followPiece][followTo][mv.Piece()][to];
            fentry += s_historyMultiplier * delta - fentry * abs(delta) / s_historyDivisor;
            pSearch->m_followTable[1][followPiece][followTo][piece][to] = static_cast<int16_t>(fentry);
        }
    }
}

/*static */void History::setKillerMove(Search * pSearch, Move mv, int ply)
{
    if (pSearch->m_killerMoves[ply][0] != mv) {
        auto tmp = pSearch->m_killerMoves[ply][0];
        pSearch->m_killerMoves[ply][0] = mv;
        pSearch->m_killerMoves[ply][1] = tmp;
    }
}

/*static */void History::fetchHistory(Search * pSearch, Move mv, int ply, HistoryHeuristics & hh)
{
    assert(ply < MAX_PLY);

    auto from = mv.From();
    auto piece = mv.Piece();
    auto to = mv.To();

    auto colour = pSearch->m_position.Side();
    hh.history = pSearch->m_history[colour][from][to];

    auto counterMove = pSearch->m_moveStack[ply - 1];
    auto counterPiece = pSearch->m_pieceStack[ply - 1];
    auto counterTo = counterMove.To();

    if (counterMove)
        hh.cmhistory = pSearch->m_followTable[0][counterPiece][counterTo][piece][to];

    if (ply > 1) {
        auto followMove = pSearch->m_moveStack[ply - 2];
        auto followPiece = pSearch->m_pieceStack[ply - 2];
        auto followTo = followMove.To();

        if (followMove)
            hh.fmhistory = pSearch->m_followTable[1][followPiece][followTo][mv.Piece()][to];
    }
}
