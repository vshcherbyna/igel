/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2002-2018 Vladimir Medvedev <vrm@bk.ru> (GreKo author)
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

#include "../moves.h"
#include "../position.h"
#include <gtest/gtest.h>

namespace unit
{

TEST(Move, Positive)
{
    MoveList mvlist;
    EXPECT_EQ(0, mvlist.Size());

    Move mv;
    EXPECT_EQ(0, mv);

    for (auto i = 1; i < 256; ++i)
    {
        EXPECT_EQ(true, mvlist.Add(mv));
        EXPECT_EQ(i, mvlist.Size());
    }

    for (auto i = 1; i < 256; ++i)
    {
        EXPECT_EQ(false, mvlist.Add(mv));
        EXPECT_EQ(255, mvlist.Size());
    }

    mvlist.Clear();
    EXPECT_EQ(0, mvlist.Size());
}

}
