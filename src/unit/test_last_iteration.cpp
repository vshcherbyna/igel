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

#include "../bitboards.h"
#include "../moves.h"
#include "../nnue.h"
#include "../search.h"
#include "../tt.h"
#include <gtest/gtest.h>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace unit
{

namespace
{

const char * TCEC_566_MOVE_16 = "r1bqnrk1/1p3p1p/3p1bp1/p1nPp2P/P3P3/R1N3N1/1P1BBPP1/2Q1K2R b K - 8 16";
const char * TCEC_566_MOVE_28 = "r1r3k1/1p5p/3p1bp1/p2Pp2n/Pn2P3/2N1B1P1/1P1K1P1R/R1N5 b - - 4 28";

bool evaluationReady()
{
#if !defined(PURE_HCE)
    return Evaluator::initEval();
#else
    return true;
#endif
}

struct Answer
{
    uint64_t searchedNodes = 0;
    uint64_t reportedNodes = 0;
    int depth = 0;
    std::string score;
    std::vector<std::string> pv;
    std::string best;
    std::string ponder;
};

Answer play(const char * fen, const char * limit, uint64_t value)
{
    InitBitboards();
    Position::InitHashNumbers();

    EXPECT_TRUE(TTable::instance().setHashSize(16, 1));
    EXPECT_TRUE(TTable::instance().clearHash(1));

    auto search = std::make_unique<Search>();
    search->clearHistory();
    search->clearKillers();
    search->clearStacks();
    search->m_principalSearcher = true;
    EXPECT_TRUE(search->setFEN(fen));

    Time time;
    EXPECT_TRUE(time.parseTime({ "go", limit, std::to_string(value) }, search->m_position.Side() == WHITE));

    std::stringstream output;
    auto console = std::cout.rdbuf(output.rdbuf());

    Answer answer;
    answer.searchedNodes = search->startSearch(time, 1, false);

    std::cout.rdbuf(console);

    std::string line, info, word;

    while (std::getline(output, line)) {
        if (line.rfind("info depth", 0) == 0)
            info = line;
        else if (line.rfind("bestmove", 0) == 0) {
            std::istringstream words(line);
            words >> word >> answer.best >> word >> answer.ponder;
        }
    }

    std::istringstream words(info);

    while (words >> word) {
        if (word == "depth")
            words >> answer.depth;
        else if (word == "score") {
            std::string kind, value;
            words >> kind >> value;
            answer.score = kind + " " + value;
        }
        else if (word == "nodes")
            words >> answer.reportedNodes;
        else if (word == "pv") {
            while (words >> word)
                answer.pv.push_back(word);
        }
    }

    return answer;
}

void expectLastCompletedIteration(const char * fen, uint64_t limit, std::map<int, Answer> & completed)
{
    auto toDepth = [&](int depth) -> const Answer & {
        auto it = completed.find(depth);
        if (it == completed.end())
            it = completed.emplace(depth, play(fen, "depth", depth)).first;
        return it->second;
    };

    const auto stopped = play(fen, "nodes", limit);
    const auto & same  = toDepth(stopped.depth);
    const auto & next  = toDepth(stopped.depth + 1);

    EXPECT_LE(same.searchedNodes, limit) << "depth " << stopped.depth << " does not complete within " << limit << " nodes";
    EXPECT_GT(next.searchedNodes, limit) << "depth " << stopped.depth + 1 << " completes within " << limit << " nodes as well";

    EXPECT_EQ(stopped.best, same.best) << "node limit " << limit;
    EXPECT_EQ(stopped.ponder, same.ponder) << "node limit " << limit;
    EXPECT_EQ(stopped.score, same.score) << "node limit " << limit;
    EXPECT_EQ(stopped.pv, same.pv) << "node limit " << limit;

    ASSERT_GT(stopped.pv.size(), 1u) << "node limit " << limit;
    EXPECT_EQ(stopped.pv[0], stopped.best);
    EXPECT_EQ(stopped.pv[1], stopped.ponder);

    EXPECT_GE(stopped.searchedNodes, limit);
    EXPECT_EQ(stopped.reportedNodes, stopped.searchedNodes);
}

}

TEST(LastCompletedIteration, TcecGame566Move16StopsInsideARootFailHigh)
{
    if (!evaluationReady())
        GTEST_SKIP() << "no usable embedded network; this check needs a real evaluation";

    std::map<int, Answer> completed;
    expectLastCompletedIteration(TCEC_566_MOVE_16, 700000, completed);
}

TEST(LastCompletedIteration, EveryNodeLimitAnswersWithTheDeepestCompletedDepth)
{
    if (!evaluationReady())
        GTEST_SKIP() << "no usable embedded network; this check needs a real evaluation";

    std::map<int, Answer> completed;

    for (uint64_t limit = 2000; limit <= 30000; limit += 2000)
        expectLastCompletedIteration(TCEC_566_MOVE_28, limit, completed);
}

}
