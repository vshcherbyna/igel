/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2019-2020 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

#ifndef TEXEL_H
#define TEXEL_H

#include "types.h"
#include "evaluate.h"
#include "position.h"

#include <memory>
#include <vector>
#include <thread>
#include <future>
#include <functional>

void onTune();
double sigmoid(double K, double S);

struct TexelParam
{
    double result;
    char fen[128];
};

class TuneWorker;

class Texel
{
public:
    Texel();
    ~Texel();
    Texel(const Texel&) = delete;
    Texel& operator=(const Texel&) = delete;

public:
    void Tune();

private:
    std::vector<TexelParam> readParams();
    double computeOptimalK(std::vector<TexelParam> & fens);
    std::vector<int> localOptimize(double K, const std::vector<int>& initialGuess, std::vector<TexelParam>& fens, size_t epoch, size_t start, size_t len);
    double completeEvaluationError(std::vector<TexelParam>& fens, double K);
    void setTuneStartTime();

private:
    U32 m_t0;
    const size_t m_maxThreads = 12;
    std::unique_ptr<std::thread[]> m_workerThreads;
    std::unique_ptr<TuneWorker[]> m_workers;
};

class TuneWorker
{
    friend class Texel;

public:
    TuneWorker() : m_exit(false), m_pos(new Position), m_evaluator(new Evaluator), K(0), m_error(0), m_finished(true) {}
    bool isTaskReady() { return !m_tasks.empty(); }
    void workerRoutine()
    {
        while (!m_exit) {
            {
                std::unique_lock<std::mutex> lk(m_mutex);
                m_cv.wait(lk, std::bind(&TuneWorker::isTaskReady, this));
                m_finished = false;

                if (m_exit)
                    return;
            }

            // we got a task, let's evaluate it

            double total = 0.0;

            for (auto const& s : m_tasks) {

                if (!m_pos->SetFEN(s.fen))
                    abort();

                double e = m_evaluator->evaluate(*m_pos.get());

                if (m_pos->Side() == BLACK)
                    e = -e;

                total += pow(s.result - sigmoid(K, e), 2);
            }

            m_error = total / m_tasks.size();

            std::unique_lock<std::mutex> l(m_mutex);
            m_finished = true;
            m_tasks.clear();
        }
    }
private:
    bool m_exit;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::vector<TexelParam> m_tasks;
    std::unique_ptr<Position> m_pos;
    std::unique_ptr<Evaluator> m_evaluator;
    double K;
    double m_error;
    bool m_finished;
};

#endif // TUNER_H
