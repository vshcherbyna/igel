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

#ifndef UCI_H
#define UCI_H

#include "moves.h"
#include "search.h"

#include <vector>
#include <string>

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

class Uci
{
    using commandParams = std::vector<std::string>;

public:
    Uci(Search & searcher) : m_searcher(searcher) { }
    ~Uci() {}

public:
    int handleCommands();
    int onBench(const char * nnue);
    static commandParams split(const std::string & s, const std::string & sep = " ");

private:
    void onUci();
    void onUciNewGame();
    void onIsready();
    void onNewGame();
    void onGo(commandParams params);
    void onStop();
    void onPonderHit();
    void onPosition(commandParams params);
    void onSetOption(commandParams params);
    void onEval();
    bool startsWith(const std::string & str, const std::string & ptrn);

private:
    Search & m_searcher;
};

#endif // UCI_H
