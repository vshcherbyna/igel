/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 1990-1992 Robert Hyatt and Tim Mann (crafty authors): lockless design of tt
*  Copyright (C) 2002-2018 Vladimir Medvedev <vrm@bk.ru> (GreKo author): general logic of tt
*  Copyright (C) 2018-2019 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

    TEntry(): m_data(0), m_key(0)
    {
    }

public:
    /*
     *     shr  bits   name   description                                          *
     *      56    8    age    age of tt table entry                                *
     *      54    2    type   0:HASH_ALPHA; 1:HASH_EXACT; 2:HASH_BETA              *
     *      30   24    move   best move from the current position, according to    *
     *                        the search at the time this position was stored      *
     *      22   8     depth  the depth of the search below this position, which   *
     *                        is used to see if we can use this entry at the       *
     *                        current position                                     *
     *       0   22    score  signed score integer value of this position          *
     *       0   64    key    64 bit hash signature, used to verify that this      *
     *                        entry goes with the current board position.          *
     *                                                                             *
     */
    void store(Move mv, EVAL score, U8 depth, U8 type, U64 hash0, U8 age)
    {
        assert(type >= 0 && type <= 2);
        assert(age >= 0 && age <= 255);
        assert(depth >= 0 && depth <= 255);
        assert(score >= -50000 && score <= 50000);
        assert(mv >= 0 && mv <= 16777216);

        m_data = age;
        m_data = (m_data << 2)  | type;
        m_data = (m_data << 24) | mv;
        m_data = (m_data << 8)  | depth;
        m_data = (m_data << 22) | (score + 65536);

        m_key = hash0 ^ m_data;
    }

    U8 age() {return (m_data >> 56);}
    U8 type() {return (m_data >> 54) & 3;}
    Move move(){ return Move((m_data >> 30) & ~0xff000000);}
    U8 depth() {return (m_data >> 22) & 0x7fff;}
    EVAL score() {return (m_data & 0x1ffff) - 65536;}

public:
    U64  m_data;
    U64  m_key;
};

class TTable
{
public:
    TTable();
    static TTable & instance();

public:
    bool setHashSize(double mb, unsigned int threads);
    bool clearHash(unsigned int threads);
    bool record(Move mv, EVAL score, U8 depth, int ply, U8 type, U64 hash0);
    TEntry * retrieve(U64 hash);
    bool increaseAge();
    void clearAge();

private:
    mutable TEntry * m_hash;
    mutable size_t m_hashSize;
    mutable unsigned int m_hashAge;
};

#endif
