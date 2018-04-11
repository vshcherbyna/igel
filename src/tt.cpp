/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 1990-1992 Robert Hyatt and Tim Mann (crafty authors): lockless design of tt
*  Copyright (C) 2002-2018 Vladimir Medvedev <vrm@bk.ru> (GreKo author): general logic of tt
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

#include "tt.h"


TTable::TTable() : m_hash(nullptr), m_hashSize(0), m_hashAge(0)
{
}

TTable & TTable::instance()
{
    static TTable instance;
    return instance;
}

bool TTable::record(Move mv, EVAL score, U8 depth, int ply, U8 type, U64 hash0)
{
    assert(m_hash);
    assert(m_hashSize);

    int index = hash0 % m_hashSize;
    TEntries & entry = m_hash[index];

    if (score > CHECKMATE_SCORE - 50 && score <= CHECKMATE_SCORE)
        score += ply;

    if (score < -CHECKMATE_SCORE + 50 && score >= -CHECKMATE_SCORE)
        score -= ply;

    for (auto i = 0; i < MAX_T_ENTRIES_PER_SLOT; ++i)
    {
        if ((entry.data[i].m_key ^ entry.data[i].m_data) == hash0)
        {
            if (depth >= entry.data[i].depth() || m_hashAge != entry.data[i].age())
            {
                entry.data[i].store(mv, score, depth, type, hash0, m_hashAge);
                return true;
            }
        }
    }

    //
    //  Find empty slot if possible
    //

    for (auto i = 0; i < MAX_T_ENTRIES_PER_SLOT; ++i)
    {
        if (!entry.data[i].m_key)
        {
            assert(!entry.data[i].m_data);
            entry.data[i].store(mv, score, depth, type, hash0, m_hashAge);

            return true;
        }
    }

    //
    //  If there are no empty slots, check if we have ones with lower depth
    //

    for (auto i = 0; i < MAX_T_ENTRIES_PER_SLOT; ++i)
    {
        if (depth >= entry.data[i].depth() || m_hashAge != entry.data[i].age())
        {
            entry.data[i].store(mv, score, depth, type, hash0, m_hashAge);
            return true;
        }
    }

    return false;
}

TEntry * TTable::retrieve(U64 hash)
{
    assert(m_hash);
    assert(m_hashSize);

    int index = hash % m_hashSize;
    TEntries & entry = m_hash[index];

    for (auto i = 0; i < MAX_T_ENTRIES_PER_SLOT; ++i)
    {
        if ((entry.data[i].m_key ^ entry.data[i].m_data) == hash)
            return &entry.data[i];
    }

    return nullptr;
}

bool TTable::clearHash()
{
    if (!m_hash)
        return false;

    memset(m_hash, 0, m_hashSize * sizeof(TEntries));
    return true;
}

bool TTable::setHashSize(double mb)
{
    if (!mb)
        return false;

    if (m_hash)
        free(m_hash);

    m_hashSize = int(1024 * 1024 * mb / sizeof(TEntries));
    m_hash = reinterpret_cast<TEntries*>(malloc(m_hashSize * sizeof(TEntries)));

    if (m_hash)
        clearHash();

    return m_hash != nullptr;
}

bool TTable::increaseAge()
{
    ++m_hashAge;
    return true;
}

void TTable::clearAge()
{
    m_hashAge = 0;
}
