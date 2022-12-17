/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2018-2023 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

#ifndef TTABLE_H
#define TTABLE_H

#include "position.h"

class TEntry
{
public:
    typedef union {
        struct {
            U32 move:24, age:8, type:2;
            I32 score:22, depth:8;
        };
        U64 raw;
    }HashEntry;
    static_assert(sizeof(HashEntry) == 8, "HashEntry must be 8 bytes");

    TEntry()
    {
        m_data.raw  = 0;
        m_key       = 0;
    }

public:

    void store(Move mv, EVAL score, I8 depth, U8 type, U64 hash0, U8 age)
    {
        assert(type >= 0 && type <= 2);
        assert(age >= 0 && age <= 255);
        assert(depth >= -128 && depth <= 127);
        assert(score >= -50000 && score <= 50000);
        assert(mv >= 0 && mv <= 16777216);

        m_data.age   = age;
        m_data.type  = type;
        m_data.move  = mv;
        m_data.depth = depth;
        m_data.score = score;

        m_key = hash0 ^ m_data.raw;

        // when debugging a multi cpu configuration these may give you a trouble:
        assert(age == m_data.age);
        assert(type == m_data.type);
        assert(mv == m_data.move);
        assert(depth == m_data.depth);
        assert(score == m_data.score);
    }

public:
    HashEntry m_data;
    U64  m_key;
};
static_assert(sizeof(TEntry) == 16, "TEntry must be 16 bytes");

struct TTCluster {
    TEntry entry[4];
};

static_assert(sizeof(TTCluster) == 64, "TTCluster must be 64 bytes");

class TTable
{
public:
    TTable();
    static TTable & instance();

public:
    bool setHashSize(double mb, unsigned int threads);
    bool clearHash(unsigned int threads);
    void record(Move mv, EVAL score, I8 depth, int ply, U8 type, U64 hash0);
    bool retrieve(U64 hash, TEntry & hentry);
    bool increaseAge();
    void clearAge();
    void prefetchEntry(U64 hash);

private:
    mutable TTCluster * m_hash;
    mutable size_t m_hashSize;
    mutable unsigned int m_hashAge;
    static constexpr uint64_t MB = 1ull << 20;

};

#endif
