//   GreKo chess engine
//   (c) 2002-2018 Vladimir Medvedev <vrm@bk.ru>
//   http://greko.su

#ifndef LEARN_H
#define LEARN_H

#include "types.h"

int    CountGames(const string& file);
bool   PgnToFen(const string& pgnFile, const string& fenFile, int minPly, int maxPly, int skipGames, int fensPerGame);
double Predict(const string& fenFile);
void   CoordinateDescent(const string& fenFile, vector<int>& x0, const vector<int>& params);
void   RandomWalk(const string& fenFile, vector<int>& x0, int limitTimeInSec, bool simulatedAnnealing, const vector<int>& params);
void   SetStartLearnTime();

#endif
