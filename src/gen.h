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
#include <chrono>

#if defined(__linux__) && !defined(__ANDROID__)
#include <sys/mman.h>
#endif

extern std::vector<std::list<Move>> g_movesBook;

class GenWorker
{
    friend class Generator;

public:
    GenWorker() : m_exit(false), m_pFile(nullptr), m_pMutex(nullptr), m_counter(0), m_search(new Search), m_finished(true), m_maxDepth(1) {}
    bool isTaskReady() { return !m_tasks.empty(); }
    void workerRoutine()
    {
        std::vector<std::string> p = { "go", "depth", std::to_string(m_maxDepth) };
        Time time;
        time.parseTime(p, true);
        RandSeed(std::chrono::high_resolution_clock::now().time_since_epoch().count());

        while (!m_exit) {

new_game:
            m_search->m_position.SetInitial();
            m_search->clearStacks();
            m_search->clearHistory();
            m_search->clearKillers();

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
                COLOR side;
            };

            std::list<genEntry> entries;

            while (!m_exit) {

                if (m_search->isGameOver(m_search->m_position, result, comment, onlyMove, legalMoves)) {

                    //
                    // the game is over, write down the results to a file
                    // use mutex since we may alter 'm_pFile' from time to time
                    //

                    m_pMutex->lock();

                    for (auto const & i : entries) {

                        if (i.ply < 400) {
                            int res = 0;

                            if (result == "1-0") {
                                if (i.side == WHITE)
                                    res = 1;
                                else
                                    res = -1;
                            }
                            else if (result == "0-1") {
                                if (i.side == BLACK)
                                    res = 1;
                                else
                                    res = -1;
                            }

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

                auto ply   = m_search->m_position.Ply();
                auto fen   = m_search->m_position.FEN();
                auto move  = MoveToStrLong(m_search->m_best);
                auto side  = m_search->m_position.Side();

                //
                // make a move, it must be legal
                //

                if (!m_search->m_position.MakeMove(m_search->m_best)) {
                    std::cout << "Unable to make a move: " << fen << " " << move << std::endl;
                    abort();
                }

                //
                // store the position
                //

                auto & entry = entries.emplace_back();

                entry.fen = fen;
                entry.move = move;
                entry.score = m_search->m_score;
                entry.ply = ply;
                entry.side = side;

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
    std::unique_ptr<Search> m_search;
    bool m_finished;
    int m_maxDepth;
    std::vector<std::string> m_tasks;
};

class Generator
{
public:
    Generator(int depth, int threads);

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
