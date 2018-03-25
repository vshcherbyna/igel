/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2018 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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
    static Time & instance();

    ~Time() = default;
    Time(const Time&) = delete;
    Time& operator=(const Time&) = delete;

public:
    enum TimeControl
    {
        Infinite,
        DepthLimit,
        NodesLimit,
        TimeLimit
    };
public:
    bool parseTime(const std::vector<std::string> & cmdline, bool whiteSide);
    TimeControl getTimeMode();
    U32 getSoftLimit();
    U32 getHardLimit();
    U32 getDepthLimit();
    U32 getNodesLimit();

private:
    void reset();
    bool evaluate();
    U32 getEnemyLowTimeBonus();

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
};

#endif // TIME_H