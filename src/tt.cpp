/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2018-2025 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

#if defined(_WIN32)
#include <malloc.h>
#endif

#if defined(__linux__) && !defined(__ANDROID__)
#include <sys/mman.h>
#endif

static TTCluster * allocClusters(size_t bytes) {
#if defined(__linux__) && !defined(__ANDROID__)
    // idea comes from Sami Kiminki as used in Ethereal
    auto * p = reinterpret_cast<TTCluster*>(aligned_alloc(2 * (1ull << 20), bytes));
    if (p)
        madvise(p, bytes, MADV_HUGEPAGE);
    return p;
#elif defined(_WIN32)
    return reinterpret_cast<TTCluster*>(_aligned_malloc(bytes, sizeof(TTCluster)));
#else
    void * p = nullptr;
    if (posix_memalign(&p, sizeof(TTCluster), bytes))
        return nullptr;
    return reinterpret_cast<TTCluster*>(p);
#endif
}

static void freeClusters(TTCluster * p)
{
#if defined(_WIN32) && !(defined(__linux__) && !defined(__ANDROID__))
    _aligned_free(p);
#else
    free(p);
#endif
}

TTable::TTable() : m_hash(nullptr), m_hashSize(0), m_hashMask(0), m_hashAge(0)
{
}

TTable & TTable::instance()
{
    static TTable instance;
    return instance;
}

void TTable::record(Move mv, EVAL score, EVAL eval, int depth, int ply, U8 type, bool pv, U64 hash0)
{
    assert(m_hash);
    assert(m_hashSize);

    size_t index = hash0 & m_hashMask;
    TTCluster & cluster = m_hash[index];
    auto * replaceEntry = &cluster.entry[0];

    //
    // the generation is a 5-bit field, so reduce the counter the same way before
    // comparing; this lets the counter wrap cleanly every 32 generations
    //

    const U8  curAge = static_cast<U8>(m_hashAge & TTEntry::GENERATION_MASK);
    const U16 key16  = TTEntry::keyOf(hash0);

    for (auto i = 0; i < TT_CLUSTER_SIZE; ++i) {
        // empty bucket or a matched hash
        if (!cluster.entry[i].isOccupied()) {
            replaceEntry = &cluster.entry[i];
            break;
        }

        if (cluster.entry[i].key() == key16) {

            //
            //  an eval-only write carries no move, no score and no depth: all it can add is
            //  the eval, so it refreshes and never displaces a real result of any generation
            //

            if (type == HASH_NONE) {
                if (eval != NO_SCORE)
                    cluster.entry[i].refreshEval(eval);

                return;
            }

            //
            //  a key match from this generation is kept unless the new result is exact or nearly as deep
            //

            if (type != HASH_EXACT && cluster.entry[i].age() == curAge && depth + 4 < cluster.entry[i].depth()) {

                //
                //  the entry stays, but a static eval it is missing is still worth having
                //

                if (eval != NO_SCORE)
                    cluster.entry[i].refreshEval(eval);

                return;
            }

            replaceEntry = &cluster.entry[i];
            break;
        }

        if ((cluster.entry[i].age() == curAge) - (replaceEntry->age() == curAge) - (cluster.entry[i].depth() < replaceEntry->depth()) < 0)
            replaceEntry = &cluster.entry[i];
    }

    if (type == HASH_NONE && replaceEntry->isOccupied() && replaceEntry->age() == curAge
        && replaceEntry->type() != HASH_NONE) {

        TTEntry * cheapest = nullptr;

        for (auto i = 0; i < TT_CLUSTER_SIZE; ++i) {
            auto & candidate = cluster.entry[i];

            if (candidate.type() == HASH_NONE || candidate.age() != curAge) {
                cheapest = &candidate;
                break;
            }
        }

        if (!cheapest)
            return;             //  every slot holds a live result; leave them all alone

        replaceEntry = cheapest;
    }

    replaceEntry->store(hash0, mv, scoreToTT(score, ply), eval, depth, type, pv, curAge);
}

bool TTable::retrieve(U64 hash, TEntry & hentry)
{
    assert(m_hash);
    assert(m_hashSize);

    hentry = TEntry{};   // a miss leaves nothing usable behind

    size_t index = hash & m_hashMask;
    auto pCluster = m_hash + index;
    const U16 key16 = TTEntry::keyOf(hash);

    for (auto i = 0; i < TT_CLUSTER_SIZE; ++i) {

        const TTEntry & entry = pCluster->entry[i];

        if (entry.isOccupied() && entry.key() == key16) {
            hentry = entry.read();
            return true;
        }
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
        freeClusters(m_hash);

    m_hashSize = static_cast<size_t>(static_cast<size_t>(1024 * 1024) * mb / sizeof(TTCluster));

    // Round down to the nearest power of 2 so we can use & instead of % in hot paths
    if (m_hashSize > 1) {
        size_t p = 1;
        while (p * 2 <= m_hashSize) p *= 2;
        m_hashSize = p;
    }
    m_hashMask = m_hashSize - 1;

    //
    //  the verification key lives in the top 16 bits of the hash, so the index must never
    //  reach that far
    //

    assert(m_hashMask < (1ull << 48));

    m_hash = allocClusters(sizeof(TTCluster) * m_hashSize);

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

    size_t index = hash & m_hashMask;
    auto pCluster = m_hash + index;

    prefetch(pCluster);
}