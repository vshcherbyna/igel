/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2019 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

#include "eval.h"
#include "eval_params.h"
#include "texel.h"
#include "moves.h"
#include "notation.h"
#include "search.h"
#include "utils.h"

#include <algorithm>
#include <numeric>
#include <random>
#include <set>

void onTune()
{
    Texel t;

    t.Tune();
}

Texel::Texel() : m_t0(GetProcTime())
{
    m_workerThreads.reset(new std::thread[m_maxThreads]);
    m_workers.reset(new TuneWorker[m_maxThreads]);

    for (size_t i = 0; i < m_maxThreads; ++i)
        m_workerThreads[i] = std::thread(&TuneWorker::workerRoutine, &m_workers[i]);
}

Texel::~Texel()
{
    for (size_t i = 0; i < m_maxThreads; ++i)
        m_workers[i].m_exit = true;
}

std::vector<TexelParam> Texel::readParams()
{
    ifstream ifs("positions.fen");
    string s;
    std::vector<TexelParam> params;
    std::set<string> unique;
    auto duplicates = 0;

    while (getline(ifs, s)) {

        TexelParam param;

        char chRes = s[0];
        param.result = 0;
        if (chRes == '1')
            param.result = 1;
        else if (chRes == '0')
            param.result = 0;
        else if (chRes == '=')
            param.result = 0.5;
        else
        {
            cout << "invalid string: " << s << endl;
            abort();
        }

        string fen = string(s.c_str() + 2);

        if (fen.length() > 127) {
            cout << "invalid length: " << fen.length() << endl;
            abort();
        }

        if (unique.find(fen) == unique.end())
            unique.insert(fen);
        else {
            ++duplicates;
            continue;
        }

        strcpy(param.fen, fen.c_str());
        params.emplace_back(param);
    }

    return params;
}

void Texel::Tune()
{
    evalWeights.resize(0);
    evalWeights.resize(NUM_PARAMS);

    auto x0 = evalWeights;
    auto params = readParams();
    auto epoch = 0;

    // texel tuning: https://www.chessprogramming.org/Texel%27s_Tuning_Method

    while (++epoch)
    {
        cout << "Epoch " << epoch << " total time: "<< (GetProcTime() - m_t0) / 1000 << " seconds" << std::endl;

        std::shuffle(std::begin(params), std::end(params), std::default_random_engine{});
        x0 = localOptimize(x0, params, epoch);

        if (x0.empty()) {
            cout << "Finished tuning after " << epoch << " epochs and " << (GetProcTime() - m_t0) / 1000 << " seconds" << endl;
            break;
        }

        std::ifstream  src("igel.txt");
        std::ofstream  dst("igel.txt_" + std::to_string(epoch));
        dst << src.rdbuf();
    }
}

void texelInfo(double y0Start, double y0, int t, int p, int epoch)
{
    int hh = t / 3600;
    int mm = (t - 3600 * hh) / 60;
    int ss = t - 3600 * hh - 60 * mm;

    cout << right;
    cout << setw(2) << setfill('0') << hh << ":";
    cout << setw(2) << setfill('0') << mm << ":";
    cout << setw(2) << setfill('0') << ss << " ";

    cout << left << "      " <<
        setw(8) << y0 << " " <<
        100 * (y0 - y0Start) / y0Start << " % " << "[" << epoch << "] " << p <<
        "          \r";

    cout << setfill(' ');
}

double sigmoid(double K, double S) {
    return 1.0 / (1.0 + pow(10.0, -K * S / 400.0));
}

double Texel::completeEvaluationError(std::vector<TexelParam> & fens, double K)
{
    const auto fensPerThread = fens.size() / m_maxThreads;
    std::vector<std::vector<TexelParam>> threadFens;

    threadFens.resize(m_maxThreads);
    size_t processedFens = 0;

    for (size_t thread = 0; thread < m_maxThreads; ++thread) {
        threadFens[thread].resize(fensPerThread);

        for (size_t fen = 0; fen < fensPerThread; ++fen, ++processedFens) {
            threadFens[thread][fen] = fens[processedFens];
        }

        m_workers[thread].m_mutex.lock();
        m_workers[thread].K = K;
        m_workers[thread].m_error = 0.0;
        m_workers[thread].m_tasks = threadFens[thread];
        m_workers[thread].m_mutex.unlock();
        m_workers[thread].m_cv.notify_one();
    }

    double total = 0.0;

    for (size_t thread = 0; thread < m_maxThreads; ++thread) {

        while (true) {
            std::unique_lock<std::mutex> lk(m_workers[thread].m_mutex);
            if (m_workers[thread].m_finished)
                break;
        }

        total += m_workers[thread].m_error;
    }

    return total / m_maxThreads;
}

double Texel::computeOptimalK(std::vector<TexelParam>& fens) {

    cout << "calculate optimal K:" << endl;

    double start = -10.0, end = 10.0, delta = 1.0;
    double curr = start, error, best = completeEvaluationError(fens, start);

    for (auto i = 0; i < 10; ++i) {
        curr = start - delta;
        while (curr < end) {
            curr = curr + delta;
            error = completeEvaluationError(fens, curr);
            if (error <= best)
                best = error, start = curr;
        }

        printf("K = %f E = %f\n", start, best);

        end = start + delta;
        start = start - delta;
        delta = delta / 10.0;
    }

    return start;
}

vector<int> Texel::localOptimize(const vector<int> & initialGuess, std::vector<TexelParam>& fens, size_t epoch)
{
    auto bestParValues = initialGuess;
    Evaluator::initEval(initialGuess);
    double K = computeOptimalK(fens);
    double bestE = completeEvaluationError(fens, K);
    auto initialError = bestE;
    auto iterationTime = GetProcTime();
    auto improved = false;

    for (size_t pi = 8; pi < initialGuess.size(); ++pi) {
        vector<int> newParValues = bestParValues;
        newParValues[pi] += 1;
        Evaluator::initEval(newParValues);
        double newE = completeEvaluationError(fens, K);
        if (newE < bestE) {
            improved = true;
            bestE = newE;
            bestParValues = newParValues;
            texelInfo(initialError, bestE, (GetProcTime() - iterationTime) / 1000, pi, epoch);
            WriteParamsToFile(bestParValues, "igel.txt");
        }
        else {
            newParValues[pi] -= 2;
            Evaluator::initEval(newParValues);
            newE = completeEvaluationError(fens, K);
            if (newE < bestE) {
                improved = true;
                bestE = newE;
                bestParValues = newParValues;
                texelInfo(initialError, bestE, (GetProcTime() - iterationTime) / 1000, pi, epoch);
                WriteParamsToFile(bestParValues, "igel.txt");
            }
        }
    }

    std::cout << std::endl;
    return improved ? bestParValues : vector<int>{};
}

void Texel::setTuneStartTime()
{
    m_t0 = GetProcTime();
}