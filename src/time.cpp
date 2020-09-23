/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
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

#include "time.h"
#include <algorithm>

Time::Time() : m_movesPlayed(0)
{
    reset();
}

Time & Time::instance()
{
    static Time instance;
    return instance;
}

void Time::reset()
{
    m_defTimeSlice  = 1000;
    m_softLimit     = 1000;
    m_hardLimit     = 1000;

    m_infinite  = false;
    m_movetime  = 0;
    m_increment = 0;
    m_moves     = 0;
    m_depth     = 0;
    m_nodes     = 0;

    m_remainingTime      = 0;
    m_remainingEnemyTime = 0;

    resetAdjustment();
}

void Time::onNewGame()
{
    m_movesPlayed = 0;
}

bool Time::parseTime(const std::vector<std::string> & cmdline, bool whiteSide)
{
    reset();

    auto limit = cmdline.size();

    for (size_t i = 1; i < limit; ++i)
    {
        if (cmdline[i] == "infinite")
        {
            m_infinite = true;
            break;
        }
        else if (cmdline[i] == "depth")
        {
            m_depth = atoi(cmdline[i + 1].c_str());
            break;
        }
        else if (cmdline[i] == "nodes")
        {
            if (i + 1 >= limit)
                return false;
            m_nodes = atoi(cmdline[i + 1].c_str());
            break;
        }
        else if (cmdline[i] == "movetime")
        {
            if (i + 1 >= limit)
                return false;

            m_movetime = atoi(cmdline[i + 1].c_str());
            break;
        }
        else if (cmdline[i] == "movestogo")
        {
            if (i + 1 >= limit)
                return false;

            m_moves = atoi(cmdline[i + 1].c_str());
            ++i;
        }
        else if (cmdline[i] == "wtime")
        {
            if (whiteSide)
                m_remainingTime = atoi(cmdline[i + 1].c_str());
            else
                m_remainingEnemyTime = atoi(cmdline[i + 1].c_str());
            ++i;
        }
        else if (cmdline[i] == "btime")
        {
            if (!whiteSide)
                m_remainingTime = atoi(cmdline[i + 1].c_str());
            else
                m_remainingEnemyTime = atoi(cmdline[i + 1].c_str());
            ++i;
        }
        else if ((cmdline[i] == "winc" && whiteSide) || (cmdline[i] == "binc" && !whiteSide))
        {
            m_increment = atoi(cmdline[i + 1].c_str());
            ++i;
        }
    }

    return evaluate();
}

bool Time::evaluate()
{
    //
    //  Absolute time control per move
    //

    if (m_movetime)
    {
        m_hardLimit = m_movetime;
        m_softLimit = m_movetime;

        return true;
    }

    if (m_remainingTime > 200)
        m_remainingTime -= 100; // allocate some reserve

    //
    //  Fixed time per number of moves
    //

    if (m_moves)
    {
        m_hardLimit = (m_remainingTime / m_moves) + (m_increment / 2) + getEnemyLowTimeBonus();

        if (m_moves == 1)
            m_hardLimit /= 2;
        else
            m_hardLimit = getMiddleGameTimeBonus(m_remainingTime, m_hardLimit);

        m_softLimit = m_hardLimit / 2;
        return true;
    }

    //
    //  Normal time control
    //

    auto bonus  = m_increment ? getEnemyLowTimeBonus() : 0;
    m_hardLimit = (m_remainingTime / 12) + (m_increment / 2) + bonus;
    m_softLimit = m_hardLimit / 4;

    return true;
}

void Time::adjust(EVAL score, int depth)
{
    //
    //  Ignore shallow depth
    //

    if (depth < 8) {
        m_prevScore = score;
        return;
    }

    //
    //  If score is dropping, request more time
    //

    if (m_prevScore > score) {
        m_softLimit *= std::min(1.0 + static_cast<double>(static_cast<double>(m_prevScore) - static_cast<double>(score)) / 80.0, 1.5);
        m_softLimit  = std::min(static_cast<int>(m_softLimit), static_cast<int>(m_hardLimit));
    }

    m_prevScore = score;
}
void Time::resetAdjustment()
{
    m_onPv  = false;
    m_prevScore = DRAW_SCORE;
}

U32 Time::getSoftLimit()
{
    return m_softLimit;
}

U32 Time::getHardLimit()
{
    return m_hardLimit;
}

U32 Time::getDepthLimit()
{
    return m_depth;
}

U32 Time::getNodesLimit()
{
    return m_nodes;
}

Time::TimeControl Time::getTimeMode()
{
    if (m_infinite)
        return TimeControl::Infinite;

    if (m_depth)
        return TimeControl::DepthLimit;

    if (m_nodes)
        return TimeControl::NodesLimit;

    return TimeControl::TimeLimit;
}

void Time::setPonderMode(bool ponder)
{
    m_infinite  = ponder;
    m_depth     = 0;
    m_nodes     = 0;
}

U32 Time::getEnemyLowTimeBonus()
{
    if (!m_remainingTime || !m_remainingEnemyTime)
        return 0;

    if (m_remainingTime <= m_remainingEnemyTime)
        return 0;

    if ((m_remainingTime / m_remainingEnemyTime) > 5)
        return 0;

    U32 diff = m_remainingTime - m_remainingEnemyTime;

    return (diff / 10);
}

U32 Time::getMiddleGameTimeBonus(U32 remainingTime, U32 hardLimit)
{
    //
    //  During first 20/40 moves allocate more time
    //

    if (m_movesPlayed < 20)
        hardLimit *= 1.50;

    return std::min(hardLimit, remainingTime);
}

bool Time::chopperMove()
{
    //
    //  Prevent instant move play in time controls of 'go movetime' and 'go infinite'
    //

    if (m_infinite || m_movetime)
        return false;

    return true;
}