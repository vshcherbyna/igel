/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2002-2018 Vladimir Medvedev <vrm@bk.ru> (GreKo author)
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

#include "eval.h"
#include "eval_params.h"
#include "tune.h"
#include "moves.h"
#include "notation.h"
#include "search.h"
#include "utils.h"

#include <algorithm>
#include <random>

void onTune()
{
    Tuner t;
    t.Tune();
}

void Tuner::Tune()
{
    //W.resize(0);
    //W.resize(NUM_PARAMS);

    auto epoch = 0;

    while (true)
    {
        ifstream ifs("games.fen");
        string s;
        std::vector<std::string> totalFens;

        while (getline(ifs, s))
            totalFens.push_back(s);

        RandSeed(time(0));

        cout << "Epoch: " << ++epoch << endl;
        cout << "Total positions: " << totalFens.size() << endl;
        cout << "Tune mode: " << "725k per 5h (orig. position)" << endl;

        auto rng = std::default_random_engine{};
        std::shuffle(std::begin(totalFens), std::end(totalFens), rng);

        //while (!totalFens.empty())
        {
            std::vector<std::string> fens = totalFens;

            /*for (int i = 0; i < 145000; ++i) {
                if (!totalFens.empty()) {
                    fens.push_back(totalFens.back());
                    totalFens.pop_back();
                }
            }*/

            vector<int> x0 = W;
            setTuneStartTime();

            vector<int> params;
            for (int i = 0; i < NUM_PARAMS; ++i)
                params.push_back(i);

            randomWalk(x0, 5 * 3600, false, params, fens);
            //cout << "Remaining positions: " << totalFens.size() << endl;
        }
    }
}

void getRandomStep(vector<int>& step, const vector<int>& params)
{
    for (size_t i = 0; i < params.size(); ++i)
    {
        int index = params[i];
        step[index] = Rand32() % 3 - 1;
    }
}

void makeStep(vector<int>& x, const vector<int>& step)
{
    assert(x.size() == step.size());

    for (size_t i = 0; i < x.size(); ++i)
        x[i] += step[i];
}

void Tuner::randomWalk(vector<int> & x0, int limitTimeInSec, bool simulatedAnnealing, const vector<int> & params, std::vector<string> & fens)
{
    RandSeed(time(0));

    Evaluator::initEval(x0);
    double y0 = predict(fens) + regularization(x0);
    double y0Start = y0;

    if (simulatedAnnealing)
        cout << "Algorithm:     simulated annealing" << endl;
    else
        cout << "Algorithm:     random walk" << endl;
    cout << "Time limit:    " << limitTimeInSec << endl;
    cout << "Initial value: " << left << y0Start << endl;

    int ssPrev = -1;

    bool empty = true;
    vector<int> bestSoFar;

    while (1)
    {
        int t = (GetProcTime() - m_t0) / 1000;

        int hh = t / 3600;
        int mm = (t - 3600 * hh) / 60;
        int ss = t - 3600 * hh - 60 * mm;

        if (ss != ssPrev)
        {
            randomWalkInfo(y0Start, y0, t);
            ssPrev = ss;
        }

        if (t >= limitTimeInSec)
        {
            cout << endl << endl;

            if (!empty)
                Evaluator::initEval(bestSoFar);

            return;
        }

        vector<int> x = x0;
        vector<int> step;
        step.resize(x.size());

        getRandomStep(step, params);
        makeStep(x, step);

        bool transition = false;
        do
        {
            int t = (GetProcTime() - m_t0) / 1000;
            if (t >= limitTimeInSec)
            {
                cout << endl << endl;

                if (!empty)
                    Evaluator::initEval(bestSoFar);

                return;
            }

            Evaluator::initEval(x);
            double y = predict(fens) + regularization(x);

            if (y <= y0)
                transition = true;
            else if (simulatedAnnealing)
            {
                double stage = 10 * t / limitTimeInSec;
                double T = 1.0e-4 * pow(10.0, -stage);
                double z = (y - y0) / T;
                double prob = exp(-z);
                int n = int(prob * 1000);
                int rand = Rand32() % 1000;
                if (rand <= n)
                    transition = true;
                else
                    transition = false;
            }
            else
                transition = false;

            if (transition)
            {
                x0 = x;
                y0 = y;

                bestSoFar = x0;
                empty = false;
                WriteParamsToFile(x0, "igel.txt");
                randomWalkInfo(y0Start, y0, t);
                makeStep(x, step);
            }
        } while (transition);
    }
}

void Tuner::randomWalkInfo(double y0Start, double y0, int t)
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
        100 * (y0 - y0Start) / y0Start << " % " <<
        "          \r";

    cout << setfill(' ');
}

double ErrSq(const string& s, double K)
{
    char chRes = s[0];
    double result = 0;
    if (chRes == '1')
        result = 1;
    else if (chRes == '0')
        result = 0;
    else if (chRes == '=')
        result = 0.5;
    else
    {
        cout << "Illegal string: " << s << endl;
        return -1;
    }

    string fen = string(s.c_str() + 2);
    std::unique_ptr<Position> pos(new Position);

    if (!pos->SetFEN(fen))
    {
        cout << "ERR FEN: " << fen << endl;
        return -1;
    }

    EVAL e = Evaluator::evaluate(*pos);

    if (pos->Side() == BLACK)
        e = -e;

    double prediction = 1. / (1. + exp(-e / K));
    double errSq = (prediction - result) * (prediction - result);

    return errSq;
}

double Tuner::predict(std::vector<string> & fens)
{
    const double K = 125;

    int n = 0;
    double errSqSum = 0;

    for (auto const& s : fens) {
        double errSq = ErrSq(s, K);
        if (errSq < 0)
            continue;

        ++n;
        errSqSum += errSq;
    }

    return sqrt(errSqSum / n);
}

void Tuner::setTuneStartTime()
{
    m_t0 = GetProcTime();
}

double Tuner::regularization(const vector<int> & x)
{
    const double lambda = 1e-10;
    double r = 0;

    for (size_t i = 0; i < x.size(); ++i)
        r += x[i] * x[i];

    return lambda * r / x.size();
}
