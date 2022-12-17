/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2018-2023 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

#ifndef TIME_H
#define TIME_H

#include "types.h"

class Time
{
public:
    Time();
    ~Time() = default;
    static Time & instance();

public:
    enum TimeControl
    {
        Infinite,
        DepthLimit,
        NodesLimit,
        TimeLimit
    };

public:
    void onNewGame();
    void adjust(EVAL score, int depth);
    void resetAdjustment();
    bool parseTime(const std::vector<std::string> & cmdline, bool whiteSide);
    TimeControl getTimeMode();
    void setPonderMode(bool ponder);
    U32 getSoftLimit();
    U32 getHardLimit();
    U32 getDepthLimit();
    U32 getNodesLimit();
    bool chopperMove();

private:
    void reset();
    bool evaluate();
    U32 getEnemyLowTimeBonus();
    U32 getMiddleGameTimeBonus(U32 remainingTime, U32 hardLimit);

private:
    U32 m_defTimeSlice;
    U32 m_softLimit;
    U32 m_hardLimit;

private:
    bool m_infinite;
    U32 m_movetime,
        m_increment,
        m_moves,
        m_depth,
        m_nodes,
        m_remainingTime,
        m_remainingEnemyTime;

private:
    bool m_onPv;
    EVAL m_prevScore;
    U32  m_movesPlayed;
};

#endif // TIME_H
