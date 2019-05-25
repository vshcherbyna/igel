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

#ifndef TUNER_H
#define TUNER_H

#include "types.h"
#include "position.h"

#include <memory>
#include <vector>

class Tuner
{
public:
    Tuner() : m_pos(new Position) { }
    Tuner(const Tuner&) = delete;
    Tuner& operator=(const Tuner&) = delete;

public:
    void Tune();

private:
    void randomWalk(vector<int> & x0, int limitTimeInSec, bool simulatedAnnealing, const vector<int> & params, std::vector<string> & fens);
    void randomWalkInfo(double y0Start, double y0, int t);
    double predict(std::vector<string> & fens);
    void setTuneStartTime();
    double regularization(const vector<int> & x);
    double errSq(const string& s, double K);

private:
    U32 m_t0;
    std::unique_ptr<Position> m_pos;
};

void onTune();

#endif // TUNER_H
