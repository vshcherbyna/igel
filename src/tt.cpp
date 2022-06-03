/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2018-2022 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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
#include "utils.h"

#include <algorithm>
#include <thread>

#if defined(__linux__) && !defined(__ANDROID__)
#include <sys/mman.h>
#endif

TTable::TTable() : m_hash(nullptr), m_hashSize(0), m_hashAge(0)
{
}

TTable & TTable::instance()
{
    static TTable instance;
    return instance;
}

void TTable::record(Move mv, EVAL score, I8 depth, int ply, U8 type, U64 hash0)
{
    assert(m_hash);
    assert(m_hashSize);

    size_t index = hash0 % m_hashSize;
    TTCluster & cluster = m_hash[index];
    auto * replaceEntry = &cluster.entry[0];

    for (auto i = 0; i < 4; ++i) {
        // empty bucket or a matched hash
        if ((!cluster.entry[i].m_key) || ((cluster.entry[i].m_key ^ cluster.entry[i].m_data.raw) == hash0)) {
            replaceEntry = &cluster.entry[i];
            break;
        }

        if ((cluster.entry[i].m_data.age == m_hashAge) - (replaceEntry->m_data.age == m_hashAge) - (cluster.entry[i].m_data.depth < replaceEntry->m_data.depth) < 0)
            replaceEntry = &cluster.entry[i];
    }

    if (score > CHECKMATE_SCORE - 50 && score <= CHECKMATE_SCORE)
        score += ply;

    if (score < -CHECKMATE_SCORE + 50 && score >= -CHECKMATE_SCORE)
        score -= ply;

    replaceEntry->store(mv, score, depth, type, hash0, m_hashAge);
}

bool TTable::retrieve(U64 hash, TEntry & hentry)
{
    assert(m_hash);
    assert(m_hashSize);

    size_t index = hash % m_hashSize;
    auto pCluster = m_hash + index;

    for (auto i = 0; i < 4; ++i) {
        hentry = pCluster->entry[i]; // make a copy of entry because a race conditon may occure between 'if' and 'return'
        if ((hentry.m_key ^ hentry.m_data.raw) == hash)
            return true;
    }

    return false;
}

bool TTable::clearHash(unsigned int threads)
{
    if (!m_hash)
        return false;

    //
    //  no optimisations required when dealing with a single thread
    //

    if (threads == 1) {
        memset(reinterpret_cast<void*>(m_hash), 0, m_hashSize * sizeof(TTCluster));
        return true;
    }

    size_t size = m_hashSize * sizeof(TTCluster);
    void * tt   = m_hash;

    //
    //  parallelize the memset across worker threads to speed up init time when using large hash (128 Gb+)
    //

    std::vector<std::thread> workers;

    for (unsigned int i = 0; i < threads; i++) {
        workers.push_back(std::thread([tt, size, i, threads]()
            {
                size_t range = size / threads;
                void* ptr = (unsigned char*)tt + (i * range);
                memset(ptr, 0, range);
            }));
    }

    //
    //  number of threads may be a non-even number, so we need to clean also remainder
    //

    if (threads > 1) {
        size_t remainder = size % threads;
        unsigned char* p = (unsigned char*)tt;
        memset(p + (size - remainder), 0, remainder);
    }

    std::for_each(workers.begin(), workers.end(), [](std::thread& t)
        {
            t.join();
        });

    return true;
}

bool TTable::setHashSize(double mb, unsigned int threads)
{
    if (!mb)
        return false;

    if (m_hash)
        free(m_hash);

    m_hashSize = static_cast<size_t>(static_cast<size_t>(1024 * 1024) * mb / sizeof(TTCluster));

#if defined(__linux__) && !defined(__ANDROID__)
    // on linux systems we align on 2MB boundaries and request Huge Pages
    // idea comes from Sami Kiminki as used in Ethereal
    m_hash = reinterpret_cast<TTCluster*>(aligned_alloc(2 * MB, sizeof(TTCluster) * m_hashSize));
    madvise(m_hash, sizeof(TTCluster) * m_hashSize, MADV_HUGEPAGE);
#else
    // otherwise, we simply allocate as usual and make no requests
    m_hash = reinterpret_cast<TTCluster*>(malloc(sizeof(TTCluster) * m_hashSize));
#endif

    clearHash(threads);

    assert(m_hash);
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

void TTable::prefetchEntry(U64 hash)
{
    assert(hash);
    assert(m_hash);
    assert(m_hashSize);

    size_t index = hash % m_hashSize;
    auto pCluster = m_hash + index;

    prefetch(pCluster);
}