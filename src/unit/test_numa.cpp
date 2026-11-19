/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2018-2026 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

#include "../numa.h"
#include <algorithm>
#include <gtest/gtest.h>

namespace unit
{

TEST(NumaParseCpuListTest, Positive)
{
    EXPECT_EQ((std::vector<int>{0, 1, 2, 5}), Numa::parseCpuList("0-2,5"));
    EXPECT_EQ((std::vector<int>{7}), Numa::parseCpuList("7"));
    EXPECT_EQ((std::vector<int>{16, 17, 32}), Numa::parseCpuList("16-17,32"));
    EXPECT_TRUE(Numa::parseCpuList("").empty());
}

TEST(NumaParseCpuListTest, Negative)
{
    for (const char * list : {"auto", "3-1", "1,,2", "-1", "1-", "0-99999", "0-3x", "0:1"})
        EXPECT_TRUE(Numa::parseCpuList(list).empty()) << list;
}

//
//  Eight nodes of 64 cpus, as on two sockets split into four nodes each
//

TEST(NumaSuggestsBindingTest, Positive)
{
    const std::vector<std::size_t> nodes(8, 64);

    EXPECT_TRUE(Numa::suggestsBinding(nodes, 512));
    EXPECT_TRUE(Numa::suggestsBinding(nodes, 33)); // more than half a node
    EXPECT_TRUE(Numa::suggestsBinding(nodes, 32)); // four for each node
    EXPECT_TRUE(Numa::suggestsBinding({64, 16}, 4)); // the small node does not count
}

TEST(NumaSuggestsBindingTest, Negative)
{
    const std::vector<std::size_t> nodes(8, 64);

    EXPECT_FALSE(Numa::suggestsBinding(nodes, 31));
    EXPECT_FALSE(Numa::suggestsBinding(nodes, 1));
    EXPECT_FALSE(Numa::suggestsBinding({256}, 256));
}

TEST(NumaDistributeTest, Positive)
{
    auto threadsOnNode = [](const std::vector<unsigned int> & plan, unsigned int node) {
        return std::count(plan.begin(), plan.end(), node);
    };

    const auto even = Numa::distribute(std::vector<std::size_t>(8, 64), 512, 3);

    EXPECT_EQ(512u, even.size());
    EXPECT_EQ(3u, even[0]);

    for (unsigned int node = 0; node < 8; ++node)
        EXPECT_EQ(64, threadsOnNode(even, node));

    const auto unequal = Numa::distribute({10, 30}, 8, 0);

    EXPECT_EQ(2, threadsOnNode(unequal, 0));
    EXPECT_EQ(6, threadsOnNode(unequal, 1));

    const auto few = Numa::distribute(std::vector<std::size_t>(8, 64), 3, 5);

    EXPECT_EQ(5u, few[0]);
    EXPECT_EQ(0u, few[1]);
    EXPECT_EQ(1u, few[2]);
}

static std::vector<std::vector<int>> socketCaches(int caches, int cores, int threads) {
    std::vector<std::vector<int>> result(caches);

    for (int cache = 0; cache < caches; ++cache)
        for (int core = 0; core < cores; ++core)
            for (int thread = 0; thread < threads; ++thread)
                result[cache].push_back(cache * cores + core + thread * caches * cores);

    return result;
}

TEST(NumaGroupCachesTest, Positive) {
    const auto epyc = Numa::groupCaches(socketCaches(16, 8, 2), 32);

    ASSERT_EQ(8u, epyc.size());

    for (const auto & group : epyc)
        EXPECT_EQ(32u, group.size());

    EXPECT_EQ(0, epyc[0][0]); // neighbours merge, in order
    EXPECT_EQ(8, epyc[0][16]);
    EXPECT_EQ(16, epyc[1][0]);

    EXPECT_EQ(1u, Numa::groupCaches(socketCaches(2, 8, 2), 32).size()); // a Ryzen 9 7950X keeps one group
    EXPECT_EQ(2u, Numa::groupCaches(socketCaches(2, 8, 2), 16).size());
}

TEST(NumaGroupCachesTest, Negative) {
    EXPECT_TRUE(Numa::groupCaches({}, 32).empty());

    //
    //  a cache with more cpus than the limit stays whole and alone
    //

    const auto groups = Numa::groupCaches({std::vector<int>(64), std::vector<int>(16), std::vector<int>(16)}, 32);

    ASSERT_EQ(2u, groups.size());
    EXPECT_EQ(64u, groups[0].size());
    EXPECT_EQ(32u, groups[1].size());
}

} // namespace unit
