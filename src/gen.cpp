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

#include "gen.h"

#include <chrono>
#include <thread>

using namespace std::chrono_literals;
std::vector<std::list<Move>> g_movesBook;

Generator::Generator(int depth, int threads, int fFash):
    m_maxDepth(depth),
    m_maxThreads(threads),
    m_maxfHash(fFash)
{
    std::cout << "Igel training data generator" << std::endl;
    std::cout << "Training depth:" << m_maxDepth << std::endl;
    std::cout << "Training threads:" << m_maxThreads << std::endl;
    std::cout << "Training fens hash:" << m_maxfHash << std::endl;

    std::cout << "Allocating fen hash table (" << fFash << "GB) ..";
    FenHashTT::instance(fFash);
    std::cout << ". Done" << std::endl;
}

void Generator::onGenerate()
{
    std::ifstream t("8moves_v3.pgn");
    std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
    std::replace(str.begin(), str.end(), '\n', ' ');

    size_t offset = 0;
    std::vector<std::string> book;

    // read book into a vector
    while (true) {
        auto pos_1 = str.find("1. ", offset);
        if (pos_1 == std::string::npos)
            break;
        auto pos_2 = str.find(" 1/2-1/2", pos_1);
        if (pos_2 == std::string::npos)
            break;
        book.emplace_back(str.substr(pos_1, pos_2 - pos_1));
        offset = pos_2 + 1;
    }

    std::cout << "Read: " << book.size() << " positions. Generating the book ..." << std::endl;

    std::unique_ptr<Position> position(new Position);

    for (const auto & i : book) {
        position->SetInitial();
        size_t moveIndex = 0;
        auto & movesList = g_movesBook.emplace_back();

        while (true) {
            auto separator = i.find(". ", moveIndex);

            if (separator == std::string::npos)
                break;

            separator += 2;
            auto move_end = i.find(" ", separator);

            if (move_end == std::string::npos)
                break;

            auto m_1 = i.substr(separator, move_end - separator);
            auto m = StrToMove(m_1, *position);

            if (position->MakeMove(m)) {
                movesList.emplace_back(m);
                auto m2 = i.find(" ", move_end);

                if (m2 == std::string::npos)
                    break;

                m2++;
                move_end = i.find(" ", m2);
                auto m_2 = i.substr(m2, move_end - m2);
                m = StrToMove(m_2, *position);
                moveIndex = move_end;

                if (position->MakeMove(m))
                    movesList.emplace_back(m);
                else {
                    std::cout << "BookGen: m2 failed (" << m_2 << ", "<< m << ")" << std::endl;
                    std::cout << i << std::endl;
                    abort();
                }
            }
            else {
                std::cout << "BookGen: m1 failed (" << m_1 << ")" << std::endl;
                abort();
            }
        }
    }

    book.clear();
    std::cout << "The book is generated: " << g_movesBook.size() << std::endl;

    m_workerThreads.reset(new std::thread[m_maxThreads]);
    m_workers.reset(new GenWorker[m_maxThreads]);

    unsigned int fileIndex = 0;

    std::ofstream myfile;
    myfile.open(std::string("data_") + std::to_string(fileIndex) + ".plain");
    std::mutex mutex;
    uint64_t fileResetFenCounter = 0;

    for (auto i = 0; i < m_maxThreads; ++i) {
        m_workers[i].m_pFile = &myfile;
        m_workers[i].m_pMutex = &mutex;
        m_workers[i].m_maxDepth = m_maxDepth;
        m_workerThreads[i] = std::thread(&GenWorker::workerRoutine, &m_workers[i]);
    }

    while (true) {

        uint64_t before_processed = 0;
        uint64_t before_skipped   = 0;

        for (auto i = 0; i < m_maxThreads; ++i) {
            before_processed += m_workers[i].m_counter;
            before_skipped   += m_workers[i].m_skipped;
        }

        std::this_thread::sleep_for(15s);

        uint64_t after_processed = 0;
        uint64_t after_skipped   = 0;

        for (auto i = 0; i < m_maxThreads; ++i) {
            after_processed += m_workers[i].m_counter;
            after_skipped   += m_workers[i].m_skipped;
        }

        std::cout << "[Processed " << after_processed << " FENs, " << ((after_processed - before_processed) / 15) << " per sec] [" ;
        std::cout << "Skipped " << after_skipped << " FENs, " << ((after_skipped - before_skipped) / 15) << " per sec]" << std::endl;

        //
        // every epoch create a new data file
        //

        if ((after_processed - fileResetFenCounter) >= 100000000) {
            std::cout << "Switch data file" << std::endl;
            mutex.lock();
            myfile.close();
            myfile.open(std::string("data_") + std::to_string(++fileIndex) + ".plain");
            mutex.unlock();
            fileResetFenCounter = after_processed;
        }
    }

    for (auto i = 0; i < m_maxThreads; ++i) {
        if (m_workerThreads[i].joinable())
            m_workerThreads[i].join();
    }
}