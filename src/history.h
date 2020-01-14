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

#ifndef HISTORY_H
#define HISTORY_H

#include "history.h"
#include "moves.h"

#include <vector>

class Search;

class History
{
    History() = delete;
    ~History() = delete;

public:
    struct HistoryHeuristics
    {
        HistoryHeuristics() : history{ 0 }, cmhistory{0}, fmhistory{0} {}
        int history;
        int cmhistory;
        int fmhistory;
    };

public:
    static void updateHistory(Search * pSearch, const std::vector<Move> & quetMoves, int ply, int bonus);
    static void setKillerMove(Search * pSearch, Move mv, int ply);
    static void fetchHistory(Search * pSearch, Move mv, int ply, HistoryHeuristics & hh);

private:
    static constexpr int s_historyMax = 400;
    static constexpr int s_historyMultiplier = 32;
    static constexpr int s_historyDivisor = 512;
};

#endif // HISTORY_H
