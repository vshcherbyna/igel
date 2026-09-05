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

#ifndef TTABLE_H
#define TTABLE_H

#include "position.h"

#include <algorithm>
#include <atomic>

const U8 HASH_ALPHA = 0;
const U8 HASH_EXACT = 1;
const U8 HASH_BETA  = 2;
const U8 HASH_NONE  = 3;

const int DEPTH_NONE       = -128;
const int DEPTH_UNSEARCHED = -127;
const int DEPTH_MAX        =  127;

inline bool isValidScore(EVAL v)    { return v > -NO_SCORE && v < NO_SCORE; }
inline bool isValidEval(EVAL v)     { return v > -DECISIVE_SCORE && v < DECISIVE_SCORE; }
inline bool isDecisiveScore(EVAL v) { return isValidScore(v) && !isValidEval(v); }

inline EVAL scoreToTT(EVAL score, int ply) {

    if (!isValidScore(score))
        return score;

    if (score >= DECISIVE_SCORE)
        return score + ply;

    if (score <= -DECISIVE_SCORE)
        return score - ply;

    return score;
}

inline EVAL scoreFromTT(EVAL score, int ply) {

    if (!isValidScore(score))
        return score;

    if (score >= DECISIVE_SCORE)
        return score - ply;

    if (score <= -DECISIVE_SCORE)
        return score + ply;

    return score;
}

struct TEntry {
    Move move  {};
    EVAL score { NO_SCORE };
    EVAL eval  { NO_SCORE };
    int  depth { DEPTH_NONE };
    U8   type  { HASH_NONE };
    bool pv    { false };
};

template <typename T>
class Relaxed
{
public:
    Relaxed() = default;
    operator T() const       { return m_v.load(std::memory_order_relaxed); }
    Relaxed & operator=(T x) { m_v.store(x, std::memory_order_relaxed); return *this; }

private:
    std::atomic<T> m_v;
};

class TTEntry 
{
public:
    static U16 keyOf(U64 hash) { return static_cast<U16>(hash >> 48); }

    static constexpr U8 GENERATION_BITS = 5;
    static constexpr U8 GENERATION_MASK = (1u << GENERATION_BITS) - 1;
    static constexpr U8 BOUND_SHIFT     = GENERATION_BITS;
    static constexpr U8 BOUND_MASK      = 0x03u << BOUND_SHIFT;
    static constexpr U8 PV_SHIFT        = BOUND_SHIFT + 2;
    static constexpr U8 PV_MASK         = 1u << PV_SHIFT;

public:
    bool isOccupied() const { return U8(m_depth8) != 0; }
    U16  key()        const { return m_key16; }
    int  depth()      const { return int(U8(m_depth8)) + DEPTH_NONE; }
    U8   type()       const { return U8((U8(m_genBound8) & BOUND_MASK) >> BOUND_SHIFT); }
    U8   age()        const { return U8(U8(m_genBound8) & GENERATION_MASK); }
    bool pv()         const { return (U8(m_genBound8) & PV_MASK) != 0; }

    TEntry read() const {

        TEntry e;

        const U8 genBound = m_genBound8;

        e.move  = Move(U32(m_move32));
        e.score = EVAL(I16(m_value16));
        e.eval  = EVAL(I16(m_eval16));
        e.depth = int(U8(m_depth8)) + DEPTH_NONE;
        e.type  = U8((genBound & BOUND_MASK) >> BOUND_SHIFT);
        e.pv    = (genBound & PV_MASK) != 0;

        return e;
    }

    void store(U64 hash0, Move mv, EVAL score, EVAL eval, int depth, U8 type, bool pv, U8 age)     {

        assert(type <= HASH_NONE);
        assert(age <= GENERATION_MASK);
        assert(isValidScore(score) || score == NO_SCORE);
        assert(isValidEval(eval) || eval == NO_SCORE);

        depth = std::max(DEPTH_UNSEARCHED, std::min(depth, DEPTH_MAX));

        const U16 k = keyOf(hash0);

        const bool samePosition = isOccupied() && k == U16(m_key16);

        if (mv || !samePosition)
            m_move32 = U32(mv);

        if (eval != NO_SCORE || !samePosition)
            m_eval16 = I16(eval);

        m_key16   = k;
        m_depth8  = U8(depth - DEPTH_NONE);
        m_value16 = I16(score);

        m_genBound8 = U8(age | U8(type << BOUND_SHIFT) | U8(U8(pv) << PV_SHIFT));
    }

    //
    //  Refresh a cached eval on an entry we decided not to overwrite
    //

    void refreshEval(EVAL eval) { m_eval16 = I16(eval); }

    //
    //  Generations are counted like hours on a clock, so 0 - 1 == 31
    //

    U8 relativeAge(U8 curAge) const { return U8((curAge - U8(m_genBound8)) & GENERATION_MASK); }

private:
    Relaxed<U16> m_key16;
    Relaxed<U8>  m_depth8;
    Relaxed<U8>  m_genBound8;
    Relaxed<U32> m_move32;
    Relaxed<I16> m_value16;
    Relaxed<I16> m_eval16;
};
static_assert(sizeof(TTEntry) == 12, "TTEntry must be 12 bytes");

const int TT_CLUSTER_SIZE = 5;

struct TTCluster {
    TTEntry entry[TT_CLUSTER_SIZE];
    char    padding[4];
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
    void record(Move mv, EVAL score, EVAL eval, int depth, int ply, U8 type, bool pv, U64 hash0);
    bool retrieve(U64 hash, TEntry & hentry);
    bool increaseAge();
    void clearAge();
    void prefetchEntry(U64 hash);

private:
    mutable TTCluster * m_hash;
    mutable size_t m_hashSize;
    mutable size_t m_hashMask;
    mutable unsigned int m_hashAge;
    static constexpr uint64_t MB = 1ull << 20;

};

#endif
