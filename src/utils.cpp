/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2002-2018 Vladimir Medvedev <vrm@bk.ru> (GreKo author)
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

#include <ctime>
#include <chrono> 

#include "bitboards.h"
#include "utils.h"

extern FILE* g_log;

U64 g_rand64 = 42;

#ifndef _MSC_VER

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>

U32 GetProcTime()
{
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (U32)((uint64_t)time.tv_sec * 1000 + time.tv_nsec / 1000000);
}

#else

#include <windows.h>

U32 GetProcTime()
{
    return GetTickCount();
}

#endif

bool Is(const string& cmd, const string& pattern, size_t minLen)
{
    return (pattern.find(cmd) == 0 && cmd.length() >= minLen);
}

void Log(const string& s)
{
    if (!g_log)
        return;

    fprintf(g_log, "%s %s\n", Timestamp().c_str(), s.c_str());
    fflush(g_log);
}

U32 Rand32()
{
    return U32(Rand64() >> 32);
}

U64 Rand64()
{
    static const U64 A = LL(2862933555777941757);
    static const U64 B = LL(3037000493);
    g_rand64 = A * g_rand64 + B;
    return g_rand64;
}

U64 Rand64(int bits)
{
    U64 r = 0;
    for (int i = 0; i < bits; ++i)
    {
        int index = Rand32() % 64;
        if (r & BB_SINGLE[index])
        {
            --i;
            continue;
        }
        r |= BB_SINGLE[index];
    }
    return r;
}

double RandDouble()
{
    double x = Rand32();
    x /= LL(0xffffffff);
    return x;
}

void RandSeed(U64 seed)
{
    g_rand64 = seed;
}

void Split(const string& s, vector<string>& tokens, const string& sep)
{
    size_t i = 0, begin = 0;
    bool inWord = false;
    tokens.clear();

    for (i = 0; i < s.length(); ++i)
    {
        if (sep.find_first_of(s[i]) != string::npos)
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
    if (inWord) tokens.push_back(s.substr(begin, i - begin));
}

string Timestamp()
{
    return {};
}