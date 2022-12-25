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

#ifndef GEN_H
#define GEN_H

#include "moves.h"
#include "moveeval.h"
#include "search.h"
#include "notation.h"
#include "utils.h"

#include <memory>
#include <vector>
#include <thread>
#include <future>
#include <list>
#include <functional>
#include <atomic>

#include <iostream>
#include <fstream>

#if defined(__linux__) && !defined(__ANDROID__)
#include <sys/mman.h>
#endif

extern std::vector<std::list<Move>> g_movesBook;

class FenHashTableEntry
{
public:
    typedef union {
        struct {
            U32 move : 24, age : 8, type : 2;
            I32 score : 22, depth : 8;
        };
        U64 raw;
    }FenHashEntry;
    static_assert(sizeof(FenHashEntry) == 8, "FenHashEntry must be 8 bytes");

    FenHashTableEntry()
    {
        m_data.raw = 0;
        m_key = 0;
    }

    void store(Move mv, EVAL score, I8 depth, U8 type, U64 hash0, U8 age)
    {
        assert(type >= 0 && type <= 2);
        assert(age >= 0 && age <= 255);
        assert(depth >= -128 && depth <= 127);
        assert(score >= -50000 && score <= 50000);
        assert(mv >= 0 && mv <= 16777216);

        m_data.age = age;
        m_data.type = type;
        m_data.move = mv;
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
    FenHashEntry m_data;
    U64  m_key;
};
static_assert(sizeof(FenHashTableEntry) == 16, "TEntry must be 16 bytes");

class FenHashTT
{
public:
    FenHashTT(int hSize) {
        size_t fenHash = static_cast<size_t>(hSize) * 1024;
        m_hashSize = static_cast<size_t>(static_cast<size_t>((1024 * 1024) * fenHash) / sizeof(FenHashTableEntry));

#if defined(__linux__) && !defined(__ANDROID__)
        // on linux systems we align on 2MB boundaries and request Huge Pages
        // idea comes from Sami Kiminki as used in Ethereal
        m_hash = reinterpret_cast<FenHashTableEntry*>(aligned_alloc(2 * MB, sizeof(FenHashTableEntry) * m_hashSize));
        madvise(m_hash, sizeof(FenHashTableEntry) * m_hashSize, MADV_HUGEPAGE);
#else
        // otherwise, we simply allocate as usual and make no requests
        m_hash = reinterpret_cast<FenHashTableEntry*>(malloc(sizeof(FenHashTableEntry) * m_hashSize));
#endif
        assert(m_hash);
        memset(reinterpret_cast<void*>(m_hash), 0, sizeof(FenHashTableEntry) * m_hashSize);
    }
    static FenHashTT & instance(int hSize) {
        static FenHashTT instance(hSize);
        return instance;
    }

public:
    void record(Move mv, EVAL score, I8 depth, int ply, U8 type, U64 hash0) {
        assert(m_hash);
        assert(m_hashSize);

        size_t index = hash0 % m_hashSize;
        FenHashTableEntry & cluster = m_hash[index];
        cluster.store(mv, score, depth, type, hash0, 0);
    }
    bool retrieve(U64 hash) {
        assert(m_hash);
        assert(m_hashSize);

        size_t index = hash % m_hashSize;
        auto pCluster = m_hash + index;

        if ((pCluster->m_key ^ pCluster->m_data.raw) == hash)
            return true;

        return false;
    }
private:
    mutable FenHashTableEntry* m_hash;
    mutable size_t m_hashSize;
    static constexpr uint64_t MB = 1ull << 20;
};

class GenWorker
{
    friend class Generator;

public:
    GenWorker() : m_exit(false), m_pFile(nullptr), m_pMutex(nullptr), m_counter(0), m_skipped(0), m_search(new Search), m_finished(true), m_maxDepth(1) {}
    bool isTaskReady() { return !m_tasks.empty(); }
    void workerRoutine()
    {
        std::vector<std::string> p = { "go", "depth", std::to_string(m_maxDepth) };
        Time time;
        time.parseTime(p, true);

        while (!m_exit) {

new_game:
            m_search->m_position.SetInitial();
            m_search->clearStacks();

            //
            // pick up a book move
            //

            auto bookIndex = Rand32() % g_movesBook.size();
            auto moveIndex = Rand32() % g_movesBook[bookIndex].size();

            unsigned long long c = 0;
            for (const auto & m : g_movesBook[bookIndex]) {
                if (!m_search->m_position.MakeMove(m)) {
                    std::cout << "Unable to make a book move: " << m_search->m_position.FEN() << " " << MoveToStrLong(m) << std::endl;
                    abort();
                }
                if (++c >= moveIndex)
                    break;
            }

            std::string result, comment;
            int legalMoves = 0;
            Move onlyMove{};

            struct genEntry {
                std::string fen;
                std::string move;
                EVAL score;
                int ply;
                bool quiet;
            };

            std::list<genEntry> entries;

            while (!m_exit) {

                if (m_search->isGameOver(m_search->m_position, result, comment, onlyMove, legalMoves)) {

                    //
                    // the game is over, write down the results to a file
                    // use mutex since we may alter 'm_pFile' from time to time
                    //

                    m_pMutex->lock();

                    for (auto const& i : entries) {
                        if (i.quiet && std::abs(i.score) <= 10000) {
                            int res = 0;
                            if (result == "1-0")
                                res = 1;
                            else if (result == "0-1")
                                res = -1;
                            else
                                res = 0; // result is a draw

                            *m_pFile << "fen " << i.fen << std::endl;
                            *m_pFile << "move " << i.move << std::endl;
                            *m_pFile << "score " << i.score << std::endl;
                            *m_pFile << "ply " << i.ply << std::endl;
                            *m_pFile << "result " << res << std::endl;
                            *m_pFile << "e" << std::endl;

                            m_counter++;
                        }
                    }

                    m_pMutex->unlock();

                    goto new_game;
                }

                m_search->m_best  = 0;
                m_search->m_score = UNKNOWN_SCORE;

                //
                // make a search with a given depth
                //

                m_search->startSearch(time, 1, false, true);

                assert(m_search->m_score != UNKNOWN_SCORE);

                if (m_search->m_score == UNKNOWN_SCORE) {
                    std::cout << "Unable to understand score: " << m_search->m_position.FEN() << " " << m_search->m_score << std::endl;
                    abort();
                }

                auto ply  = m_search->m_position.Ply();
                auto fen  = m_search->m_position.FEN();
                auto move = MoveToStrLong(m_search->m_best);

                //
                // make a move, it must be legal
                //

                if (!m_search->m_position.MakeMove(m_search->m_best)) {
                    std::cout << "Unable to make a move: " << fen << " " << move << std::endl;
                    abort();
                }

                auto hash = m_search->m_position.Hash();

                //
                // avoid duplicated fen entries
                //

                if (FenHashTT::instance(0).retrieve(hash) == false) {

                    //
                    // store the position
                    //

                    auto & entry = entries.emplace_back();

                    entry.fen = fen;
                    entry.move = move;
                    entry.score = m_search->m_score;
                    entry.ply = ply;
                    entry.quiet = MoveEval::isTacticalMove(m_search->m_best) == false;

                    FenHashTT::instance(0).record(m_search->m_best, m_search->m_score, m_search->m_depth, ply, 0, hash);
                }
                else
                    m_skipped++;

                if (m_exit)
                    return;
            }
        }
    }
private:
    bool m_exit;
    std::ofstream * m_pFile;
    std::mutex * m_pMutex;
    uint64_t m_counter;
    uint64_t m_skipped;
    std::unique_ptr<Search> m_search;
    bool m_finished;
    int m_maxDepth;
    std::vector<std::string> m_tasks;
};

class Generator
{
public:
    Generator(int depth, int threads, int fFash);

public:
    void onGenerate();

private:
    int m_maxDepth;
    int m_maxThreads;
    int m_maxfHash;
    std::unique_ptr<std::thread[]> m_workerThreads;
    std::unique_ptr<GenWorker[]> m_workers;
};

#endif // GEN_H
