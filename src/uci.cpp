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

#include "uci.h"
#include "time.h"
#include "notation.h"

#include "fathom/tbprobe.h"

#include <iostream>
#include <sstream>

const std::string VERSION = "1.8.0";

const int MIN_HASH_SIZE = 1;
const int MAX_HASH_SIZE = 131072;
const int DEFAULT_HASH_SIZE = 128;

const int MIN_THREADS = 1;
const int MAX_THREADS = 128;
const int DEFAULT_THREADS = 1;

// Check windows
#if _WIN32 || _WIN64
#if _WIN64
#define ENV64BIT
#else
#define ENV32BIT
#endif
#endif

// Check GCC
#if __GNUC__
#if __x86_64__ || __ppc64__
#define ENV64BIT
#else
#define ENV32BIT
#endif
#endif

int Uci::handleCommands()
{
    std::cout << "Igel " << VERSION << " by V. Medvedev, V. Shcherbyna" << std::endl;

    if (!TTable::instance().setHashSize(DEFAULT_HASH_SIZE)) {
        cout << "Fatal error: unable to allocate memory for transposition table" << endl;
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
    }

    return 0;
}

void Uci::onUci()
{
    std::cout << "id name Igel " << VERSION;

#if defined(ENV64BIT)
    std::cout << " 64 POPCNT";
#endif

    std::cout << std::endl << "id author V. Medvedev, V. Shcherbyna" << std::endl;

    std::cout << "option name Hash type spin"   <<
        " default " << DEFAULT_HASH_SIZE        <<
        " min "     << MIN_HASH_SIZE            <<
        " max "     << MAX_HASH_SIZE            << std::endl;

    std::cout << "option name Threads type spin"    <<
        " default " << DEFAULT_THREADS              <<
        " min "     << MIN_THREADS                  <<
        " max "     << MAX_THREADS                  << std::endl;

    std::cout << "option name SyzygyPath type string default <empty>" << std::endl;

    std::cout << "option name SyzygyProbeDepth type spin" <<
        " default "     << 1        <<
        " min "         << 1        <<
        " max "         << MAX_PLY  << std::endl;

    cout << "option name Ponder type check" <<
        " default false" << endl;

    std::cout << "uciok" << std::endl;
}

void Uci::onUciNewGame()
{
    m_searcher.m_position.SetInitial();

    TTable::instance().clearHash();
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
    //time.onPlayedMove();

    m_searcher.startPrincipalSearch(time, params[1] == "ponder");
}

void Uci::onStop()
{
    m_searcher.stopPrincipalSearch();
}

void Uci::onPonderHit()
{
    m_searcher.setPonderHit();
    onStop();
}

void Uci::onPosition(commandParams params)
{
    if (params.size() < 2) {
        std::cout << "Fatal error: invalid parameters for position command" << endl;
        return;
    }

    assert(params[0] == "position");
    size_t movesTag = 0;

    if (params[1] == "fen") {
        string fen = "";
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
        m_searcher.m_position.SetFEN(fen);
    }
    else if (params[1] == "startpos") {
        m_searcher.m_position.SetInitial();
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
            m_searcher.m_position.MakeMove(mv);
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
        if (!TTable::instance().setHashSize(atoi(value.c_str()))) {
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
    else if (name == "SyzygyPath")
        tb_init(value.c_str());
    else if (name == "SyzygyProbeDepth")
        m_searcher.setSyzygyDepth(atoi(value.c_str()));
    else if (name == "Ponder") {
        // nothing to do, we are stateless here
    }
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

    TTable::instance().clearHash();
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