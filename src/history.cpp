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

#include "history.h"
#include "search.h"

History::History()
{
}

/*static */void History::updateHistory(const MoveList & moves, int ply)
{
    auto count = moves.Size();
    for (size_t i = 0; i < count; ++i) {

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
