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

#include "uci.h"
#include "time.h"
#include "notation.h"
#include "texel.h"
#include "evaluate.h"
#include "utils.h"

#if defined (SYZYGY_SUPPORT)
#include "fathom/tbprobe.h"
#endif

#include <iostream>
#include <sstream>

const std::string VERSION = "2.7.0"; //"2.7-dev-2";

#if defined(ENV64BIT)
    #if defined(_BTYPE)
        #if _BTYPE==0
            const std::string ARCHITECTURE = " 64 POPCNT";
        #else
            const std::string ARCHITECTURE = " 64 BMI2";
    #endif
    #else
        const std::string ARCHITECTURE = " 64";
    #endif
#else
    const std::string ARCHITECTURE = " CUSTOM";
#endif

#if defined(__linux__) && !defined(__ANDROID__)
const int MIN_HASH_SIZE = 2;
#else
const int MIN_HASH_SIZE = 1;
#endif

const int DEFAULT_HASH_SIZE = 128;
const int MAX_HASH_SIZE     = 1048576;

const int DEFAULT_THREADS = 1;
const int MIN_THREADS     = 1;
const int MAX_THREADS     = 1024;

int Uci::handleCommands()
{
    std::cout << "Igel " << VERSION << ARCHITECTURE << " by V. Medvedev, V. Shcherbyna" << std::endl;

    if (!TTable::instance().setHashSize(DEFAULT_HASH_SIZE, DEFAULT_THREADS)) {
        std::cout << "Fatal error: unable to allocate memory for transposition table" << std::endl;
        return 1;
    }

    while (true) {
        std::string cmd;
        std::getline(std::cin, cmd);

        if (startsWith(cmd, "go"))
            onGo(split(cmd));
        else if (startsWith(cmd, "position"))
            onPosition(split(cmd));
        else if (startsWith(cmd, "setoption"))
            onSetOption(split(cmd));
        else if (startsWith(cmd, "isready"))
            onIsready();
        else if (startsWith(cmd, "stop"))
            onStop();
        else if (startsWith(cmd, "ponderhit"))
            onPonderHit();
        else if (startsWith(cmd, "quit"))
            exit(0);
        else if (startsWith(cmd, "ucinewgame"))
            onUciNewGame();
        else if (startsWith(cmd, "uci"))
            onUci();
        else if (startsWith(cmd, "tune"))
            onTune();
        else if (startsWith(cmd, "eval"))
            onEval();
        else if (startsWith(cmd, "bench"))
            onBench();
        else {
            std::cout << "Unknown command. Good bye." << std::endl;
            exit(0);
        }
    }

    return 0;
}

void Uci::onUci()
{
    std::cout << "id name Igel " << VERSION << ARCHITECTURE << std::endl;
    std::cout << "id author V. Medvedev, V. Shcherbyna" << std::endl;

    std::cout << "option name Hash type spin"   <<
        " default " << DEFAULT_HASH_SIZE        <<
        " min "     << MIN_HASH_SIZE            <<
        " max "     << MAX_HASH_SIZE            << std::endl;

    std::cout << "option name Threads type spin"    <<
        " default " << DEFAULT_THREADS              <<
        " min "     << MIN_THREADS                  <<
        " max "     << MAX_THREADS                  << std::endl;

#if defined (SYZYGY_SUPPORT)
    std::cout << "option name SyzygyPath type string default <empty>" << std::endl;

    std::cout << "option name SyzygyProbeDepth type spin" <<
        " default "     << 1        <<
        " min "         << 1        <<
        " max "         << MAX_PLY  << std::endl;
#endif

    std::cout << "option name Ponder type check" <<
        " default false" << std::endl;

    std::cout << "option name Skill Level type spin" <<
        " default " << DEFAULT_LEVEL <<
        " min "		<< MIN_LEVEL	<<
        " max "		<< MAX_LEVEL << std::endl;

#if defined(EVAL_NNUE)
    std::cout << "option name EvalFile type string default ./eval/nn.bin" << std::endl;
#endif

    std::cout << "uciok" << std::endl;
}

void Uci::onUciNewGame()
{
    m_searcher.m_position.SetInitial();

    TTable::instance().clearHash(m_searcher.getThreadsCount());
    TTable::instance().clearAge();

    m_searcher.clearHistory();
    m_searcher.clearKillers();
    m_searcher.clearStacks();

    Time::instance().onNewGame();
}

void Uci::onGo(commandParams params)
{
    auto & time = Time::instance();

    if (!time.parseTime(params, m_searcher.m_position.Side() == WHITE)) {
        std::cout << "Fatal error: invalid parameters for go command" << std::endl;
        return;
    }

    assert(params[0] == "go");

    TTable::instance().increaseAge();
    m_searcher.startPrincipalSearch(time, params[1] == "ponder");
}

void Uci::onStop()
{
    m_searcher.stopPrincipalSearch();
}

void Uci::onPonderHit()
{
    m_searcher.setPonderHit();
}

void Uci::onEval()
{
    std::unique_ptr<Evaluator> evaluator(new Evaluator);
    std::cout << "eval: " << evaluator->evaluate(m_searcher.m_position) << std::endl;
}

// benchmark positions from Ethereal
static const char* benchmarkPositions[] = {
    #include "bench.csv"
    ""
};

int Uci::onBench()
{
    std::cout << "Running benchmark" << std::endl;

    auto& time = Time::instance();

    if (!TTable::instance().setHashSize(16, 1)) {
        std::cout << "Fatal error: unable to allocate 16 Mb for transposition table" << std::endl;
        abort();
    }

    onUciNewGame();

    m_searcher.m_principalSearcher = true;

    uint64_t sumNodes = 0;
    auto start = GetProcTime();
    commandParams p = { "go", "depth", "11" };

    for (auto i = 0; strcmp(benchmarkPositions[i], ""); i++) {
        if (!m_searcher.m_position.SetFEN(benchmarkPositions[i]))
            abort();

        if (!time.parseTime(p, m_searcher.m_position.Side() == WHITE)) {
            std::cout << "Fatal error: invalid parameters for go command" << std::endl;
            abort();
        }

        sumNodes += m_searcher.startSearch(time, 1, false, true);
        onUciNewGame();
    }

    std::cout << "Time  : " << (GetProcTime() - start) << std::endl;
    std::cout << "Nodes : " << sumNodes << std::endl;
    std::cout << "NPS   : " << static_cast<int>(sumNodes / ((GetProcTime() - start) / 1000.0)) << std::endl;

    return static_cast<int>(sumNodes);
}

void Uci::onPosition(commandParams params)
{
    if (params.size() < 2) {
        std::cout << "Fatal error: invalid parameters for position command" << std::endl;
        return;
    }

    assert(params[0] == "position");
    size_t movesTag = 0;

    if (params[1] == "fen") {
        std::string fen = "";
        for (size_t i = 2; i < params.size(); ++i) {
            if (params[i] == "moves")
            {
                movesTag = i;
                break;
            }
            if (!fen.empty())
                fen += " ";
            fen += params[i];
        }
        m_searcher.setFEN(fen);
    }
    else if (params[1] == "startpos") {
        m_searcher.setInitialPosition();
        for (size_t i = 2; i < params.size(); ++i) {
            if (params[i] == "moves")
            {
                movesTag = i;
                break;
            }
        }
    }

    if (movesTag) {
        for (size_t i = movesTag + 1; i < params.size(); ++i) {
            Move mv = StrToMove(params[i], m_searcher.m_position);
            m_searcher.makeMove(mv);
        }
    }
}

void Uci::onSetOption(commandParams params)
{ 
    if (params.size() < 5) {
        std::cout << "Fatal error: invalid parameters for setoption command" << std::endl;
        return;
    }

    if (params[1] != "name" && params[3] != "value") {
        std::cout << "Fatal error: invalid parameters for setoption command" << std::endl;
        return;
    }

    assert(params[0] == "setoption");
    auto name   = params[2];
    auto value  = params[4];

    if (name == "Hash") {
        if (!TTable::instance().setHashSize(atoi(value.c_str()), m_searcher.getThreadsCount())) {
            std::cout << "Fatal error: unable to allocate memory for transposition table" << std::endl;
            exit(1);
        }
    }
    else if (name == "Threads") {
        auto threads = atoi(value.c_str());

        if (threads > MAX_THREADS || threads < MIN_THREADS)
            std::cout << "Unable set threads value. Make sure number is correct" << std::endl;
        m_searcher.setThreadCount(threads - 1);
    }
    else if (name == "Skill Level") {
        auto level = atoi(value.c_str());

        if (level > MAX_LEVEL || MIN_LEVEL < 0)
            std::cout << "Unable set level value. Make sure number is correct" << std::endl;
        m_searcher.setLevel(level);
    }
#if defined (SYZYGY_SUPPORT)
    else if (name == "SyzygyPath")
        tb_init(value.c_str());
    else if (name == "SyzygyProbeDepth")
        m_searcher.setSyzygyDepth(atoi(value.c_str()));
#if defined(EVAL_NNUE)
    else if (name == "EvalFile") {
        if (!Eval::NNUE::load_eval_file(value)) {
            std::cout << "Unable to set EvalFile. Aborting" << std::endl;
            abort();
        }
        else
            std::cout << "Using EvalFile " << value << std::endl;
#endif
    }
#endif
    else if (name == "Ponder")
        ; // nothing to do, we are stateless here
    else
        std::cout << "Unknown option " << name << std::endl;
}

void Uci::onIsready()
{
    m_searcher.isReady();
}

void Uci::onNewGame()
{
    m_searcher.m_position.SetInitial();

    TTable::instance().clearHash(m_searcher.getThreadsCount());
    TTable::instance().clearAge();

    m_searcher.clearHistory();
    m_searcher.clearKillers();
}

/*static */Uci::commandParams Uci::split(const std::string & s, const std::string & sep)
{
    size_t i = 0, begin = 0;
    bool inWord = false;
    std::vector<std::string> tokens;

    for (i = 0; i < s.length(); ++i)
    {
        if (sep.find_first_of(s[i]) != std::string::npos)
        {
            // separator character
            if (inWord)
                tokens.push_back(s.substr(begin, i - begin));
            inWord = false;
        }
        else
        {
            // non-separator character
            if (!inWord)
                begin = i;
            inWord = true;
        }
    }

    if (inWord) 
        tokens.push_back(s.substr(begin, i - begin));

    return tokens;
}

bool Uci::startsWith(const std::string & str, const std::string & ptrn)
{
    return !str.find(ptrn);
}
