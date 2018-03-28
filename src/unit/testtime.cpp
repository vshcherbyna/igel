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

#include "../time.h"
#include <gtest/gtest.h>

namespace unit
{


TEST(Defaults, Positive)
{
    Time t;
    EXPECT_EQ(t.getHardLimit(), 1000);
    EXPECT_EQ(t.getHardLimit(), 1000);
    EXPECT_EQ(Time::TimeControl::TimeLimit, Time::instance().getTimeMode());
}

TEST(Infinite, Positive)
{
    std::vector<std::string> cmd;

    cmd.push_back("go");
    cmd.push_back("infinite");

    EXPECT_EQ(true, Time::instance().parseTime(cmd, true));
    EXPECT_EQ(Time::TimeControl::Infinite, Time::instance().getTimeMode());
}

TEST(Depth, Positive)
{
    std::vector<std::string> cmd;

    cmd.push_back("go");
    cmd.push_back("depth");
    cmd.push_back("1");

    EXPECT_EQ(true, Time::instance().parseTime(cmd, true));
    EXPECT_EQ(Time::TimeControl::DepthLimit, Time::instance().getTimeMode());
    EXPECT_EQ(1, Time::instance().getDepthLimit());
}

TEST(Nodes, Positive)
{
    std::vector<std::string> cmd;

    cmd.push_back("go");
    cmd.push_back("nodes");
    cmd.push_back("13");

    EXPECT_EQ(true, Time::instance().parseTime(cmd, true));
    EXPECT_EQ(Time::TimeControl::NodesLimit, Time::instance().getTimeMode());
    EXPECT_EQ(13, Time::instance().getNodesLimit());
}

TEST(Movetime, Positive)
{
    std::vector<std::string> cmd;

    cmd.push_back("go");
    cmd.push_back("movetime");
    cmd.push_back("100");

    EXPECT_EQ(true, Time::instance().parseTime(cmd, true));
    EXPECT_EQ(Time::TimeControl::TimeLimit, Time::instance().getTimeMode());
    EXPECT_EQ(true, Time::instance().getHardLimit() == 100);
    EXPECT_EQ(true, Time::instance().getSoftLimit() == 100);

    EXPECT_EQ(true, Time::instance().parseTime(cmd, false));
    EXPECT_EQ(Time::TimeControl::TimeLimit, Time::instance().getTimeMode());
    EXPECT_EQ(true, Time::instance().getHardLimit() == 100);
    EXPECT_EQ(true, Time::instance().getSoftLimit() == 100);
}

TEST(Movetime, Negative)
{
    std::vector<std::string> cmd;

    cmd.push_back("go");
    cmd.push_back("movetime");

    EXPECT_EQ(false, Time::instance().parseTime(cmd, true));
    EXPECT_EQ(Time::TimeControl::TimeLimit, Time::instance().getTimeMode());
    EXPECT_EQ(true, Time::instance().getHardLimit() == 1000);
    EXPECT_EQ(true, Time::instance().getSoftLimit() == 1000);

    EXPECT_EQ(false, Time::instance().parseTime(cmd, false));
    EXPECT_EQ(Time::TimeControl::TimeLimit, Time::instance().getTimeMode());
    EXPECT_EQ(true, Time::instance().getHardLimit() == 1000);
    EXPECT_EQ(true, Time::instance().getSoftLimit() == 1000);
}

TEST(MovesToGo, Positive)
{
    std::vector<std::string> cmd;

    cmd.push_back("go");
    cmd.push_back("wtime");
    cmd.push_back("60000");
    cmd.push_back("btime");
    cmd.push_back("60000");
    cmd.push_back("movestogo");
    cmd.push_back("33");

    EXPECT_EQ(true, Time::instance().parseTime(cmd, true));
    EXPECT_EQ(Time::TimeControl::TimeLimit, Time::instance().getTimeMode());
    EXPECT_EQ(true, Time::instance().getHardLimit() == 1818);
    EXPECT_EQ(true, Time::instance().getSoftLimit() == (1818 / 2));

    EXPECT_EQ(true, Time::instance().parseTime(cmd, false));
    EXPECT_EQ(Time::TimeControl::TimeLimit, Time::instance().getTimeMode());
    EXPECT_EQ(true, Time::instance().getHardLimit() == 1818);
    EXPECT_EQ(true, Time::instance().getSoftLimit() == (1818 / 2));
}

TEST(MovesToGo, Negative)
{
    std::vector<std::string> cmd;

    cmd.push_back("go");
    cmd.push_back("wtime");
    cmd.push_back("60000");
    cmd.push_back("btime");
    cmd.push_back("60000");
    cmd.push_back("movestogo");

    EXPECT_EQ(false, Time::instance().parseTime(cmd, true));
    EXPECT_EQ(Time::TimeControl::TimeLimit, Time::instance().getTimeMode());
    EXPECT_EQ(true, Time::instance().getHardLimit() == 1000);
    EXPECT_EQ(true, Time::instance().getSoftLimit() == 1000);

    EXPECT_EQ(false, Time::instance().parseTime(cmd, false));
    EXPECT_EQ(Time::TimeControl::TimeLimit, Time::instance().getTimeMode());
    EXPECT_EQ(true, Time::instance().getHardLimit() == 1000);
    EXPECT_EQ(true, Time::instance().getSoftLimit() == 1000);
}

TEST(MovesToGoIncrement, Positive)
{
    std::vector<std::string> cmd;

    cmd.push_back("go");
    cmd.push_back("wtime");
    cmd.push_back("60000");
    cmd.push_back("btime");
    cmd.push_back("60000");

    cmd.push_back("winc");
    cmd.push_back("5000");
    cmd.push_back("binc");
    cmd.push_back("5000");

    cmd.push_back("movestogo");
    cmd.push_back("33");

    EXPECT_EQ(true, Time::instance().parseTime(cmd, true));
    EXPECT_EQ(Time::TimeControl::TimeLimit, Time::instance().getTimeMode());
    EXPECT_EQ(true, Time::instance().getHardLimit() == 4318);
    EXPECT_EQ(true, Time::instance().getSoftLimit() == (4318 / 2));
}

TEST(NormalControlNoIncrement, Positive)
{
    std::vector<std::string> cmd;

    cmd.push_back("go");
    cmd.push_back("wtime");
    cmd.push_back("60000");
    cmd.push_back("btime");
    cmd.push_back("60000");

    EXPECT_EQ(true, Time::instance().parseTime(cmd, true));
    EXPECT_EQ(Time::TimeControl::TimeLimit, Time::instance().getTimeMode());
    EXPECT_EQ(true, Time::instance().getHardLimit() == 30000);
    EXPECT_EQ(true, Time::instance().getSoftLimit() == 600);

    EXPECT_EQ(true, Time::instance().parseTime(cmd, false));
    EXPECT_EQ(Time::TimeControl::TimeLimit, Time::instance().getTimeMode());
    EXPECT_EQ(true, Time::instance().getHardLimit() == 30000);
    EXPECT_EQ(true, Time::instance().getSoftLimit() == 600);
}

TEST(NormalControlNoIncrementEnemyHasMoreTime, Positive)
{
    std::vector<std::string> cmd;

    cmd.push_back("go");
    cmd.push_back("wtime");
    cmd.push_back("60000");
    cmd.push_back("btime");
    cmd.push_back("70000");

    EXPECT_EQ(true, Time::instance().parseTime(cmd, true));
    EXPECT_EQ(Time::TimeControl::TimeLimit, Time::instance().getTimeMode());
    EXPECT_EQ(true, Time::instance().getHardLimit() == 30000);
    EXPECT_EQ(true, Time::instance().getSoftLimit() == 600);

    cmd.clear();

    cmd.push_back("go");
    cmd.push_back("wtime");
    cmd.push_back("70000");
    cmd.push_back("btime");
    cmd.push_back("60000");

    EXPECT_EQ(true, Time::instance().parseTime(cmd, false));
    EXPECT_EQ(Time::TimeControl::TimeLimit, Time::instance().getTimeMode());
    EXPECT_EQ(true, Time::instance().getHardLimit() == 30000);
    EXPECT_EQ(true, Time::instance().getSoftLimit() == 600);
}

TEST(NormalControlNoIncrementEnemyHasLessTime, Positive)
{
    std::vector<std::string> cmd;

    cmd.push_back("go");
    cmd.push_back("wtime");
    cmd.push_back("60000");
    cmd.push_back("btime");
    cmd.push_back("50000");

    EXPECT_EQ(true, Time::instance().parseTime(cmd, true));
    EXPECT_EQ(Time::TimeControl::TimeLimit, Time::instance().getTimeMode());
    EXPECT_EQ(true, Time::instance().getHardLimit() == 30000);
    EXPECT_EQ(true, Time::instance().getSoftLimit() == 622);

    cmd.clear();
    cmd.push_back("go");
    cmd.push_back("wtime");
    cmd.push_back("50000");
    cmd.push_back("btime");
    cmd.push_back("60000");

    EXPECT_EQ(true, Time::instance().parseTime(cmd, false));
    EXPECT_EQ(Time::TimeControl::TimeLimit, Time::instance().getTimeMode());
    EXPECT_EQ(true, Time::instance().getHardLimit() == 30000);
    EXPECT_EQ(true, Time::instance().getSoftLimit() == 622);
}

TEST(NormalControlIncrement, Positive)
{
    std::vector<std::string> cmd;

    cmd.push_back("go");
    cmd.push_back("wtime");
    cmd.push_back("60000");
    cmd.push_back("btime");
    cmd.push_back("60000");

    cmd.push_back("winc");
    cmd.push_back("2000");
    cmd.push_back("binc");
    cmd.push_back("1000");

    EXPECT_EQ(true, Time::instance().parseTime(cmd, true));
    EXPECT_EQ(Time::TimeControl::TimeLimit, Time::instance().getTimeMode());
    EXPECT_EQ(true, Time::instance().getHardLimit() == 30000);
    EXPECT_EQ(true, Time::instance().getSoftLimit() == 1600);

    cmd.clear();
    cmd.push_back("go");
    cmd.push_back("wtime");
    cmd.push_back("60000");
    cmd.push_back("btime");
    cmd.push_back("60000");

    cmd.push_back("winc");
    cmd.push_back("2000");
    cmd.push_back("binc");
    cmd.push_back("1000");

    EXPECT_EQ(true, Time::instance().parseTime(cmd, false));
    EXPECT_EQ(Time::TimeControl::TimeLimit, Time::instance().getTimeMode());
    EXPECT_EQ(true, Time::instance().getHardLimit() == 30000);
    EXPECT_EQ(true, Time::instance().getSoftLimit() == 1100);
}

TEST(NormalControlIncrementEnemyHasMoreTime, Positive)
{
    std::vector<std::string> cmd;

    cmd.push_back("go");
    cmd.push_back("wtime");
    cmd.push_back("60000");
    cmd.push_back("btime");
    cmd.push_back("70000");

    cmd.push_back("winc");
    cmd.push_back("2000");
    cmd.push_back("binc");
    cmd.push_back("1000");

    EXPECT_EQ(true, Time::instance().parseTime(cmd, true));
    EXPECT_EQ(Time::TimeControl::TimeLimit, Time::instance().getTimeMode());
    EXPECT_EQ(true, Time::instance().getHardLimit() == 30000);
    EXPECT_EQ(true, Time::instance().getSoftLimit() == 1600);

    cmd.clear();

    cmd.push_back("go");
    cmd.push_back("wtime");
    cmd.push_back("70000");
    cmd.push_back("btime");
    cmd.push_back("60000");

    cmd.push_back("winc");
    cmd.push_back("2000");
    cmd.push_back("binc");
    cmd.push_back("1000");

    EXPECT_EQ(true, Time::instance().parseTime(cmd, false));
    EXPECT_EQ(Time::TimeControl::TimeLimit, Time::instance().getTimeMode());
    EXPECT_EQ(true, Time::instance().getHardLimit() == 30000);
    EXPECT_EQ(true, Time::instance().getSoftLimit() == 1100);
}

TEST(NormalControlIncrementEnemyHasLessTime, Positive)
{
    std::vector<std::string> cmd;

    cmd.push_back("go");
    cmd.push_back("wtime");
    cmd.push_back("60000");
    cmd.push_back("btime");
    cmd.push_back("40000");

    cmd.push_back("winc");
    cmd.push_back("2000");
    cmd.push_back("binc");
    cmd.push_back("1000");

    EXPECT_EQ(true, Time::instance().parseTime(cmd, true));
    EXPECT_EQ(Time::TimeControl::TimeLimit, Time::instance().getTimeMode());
    EXPECT_EQ(true, Time::instance().getHardLimit() == 30000);
    EXPECT_EQ(true, Time::instance().getSoftLimit() == 1644);

    cmd.clear();

    cmd.push_back("go");
    cmd.push_back("wtime");
    cmd.push_back("30000");
    cmd.push_back("btime");
    cmd.push_back("60000");

    cmd.push_back("winc");
    cmd.push_back("2000");
    cmd.push_back("binc");
    cmd.push_back("1000");

    EXPECT_EQ(true, Time::instance().parseTime(cmd, false));
    EXPECT_EQ(Time::TimeControl::TimeLimit, Time::instance().getTimeMode());
    EXPECT_EQ(true, Time::instance().getHardLimit() == 30000);
    EXPECT_EQ(true, Time::instance().getSoftLimit() == 1166);
}

}
