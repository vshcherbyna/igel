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

#ifndef LEARN_H
#define LEARN_H

#include "types.h"

int    CountGames(const string& file);
bool   PgnToFen(const string& pgnFile, const string& fenFile, int minPly, int maxPly, int skipGames, int fensPerGame);
double Predict(const string& fenFile);
void   CoordinateDescent(const string& fenFile, vector<int>& x0, const vector<int>& params);
void   RandomWalk(const string& fenFile, vector<int>& x0, int limitTimeInSec, bool simulatedAnnealing, const vector<int>& params);
void   SetStartLearnTime();
void    onTune();

#endif
