/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2002-2018 Vladimir Medvedev <vrm@bk.ru> (GreKo author)
*  Copyright (C) 2018 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

#ifndef EVAL_H
#define EVAL_H

#include "eval_params.h"
#include "position.h"

const EVAL VAL_P = 100;
const EVAL VAL_N = 400;
const EVAL VAL_B = 400;
const EVAL VAL_R = 600;
const EVAL VAL_Q = 1200;
const EVAL VAL_K = 20000;

EVAL Evaluate(const Position& pos, EVAL alpha, EVAL beta);
void InitEval();
void InitEval(const vector<int>& x);

struct PawnHashEntry
{
	PawnHashEntry() : m_pawnHash(0) {}
	void Read(const Position& pos);

	U32 m_pawnHash;
	I8  m_ranks[10][2];

	U64 m_passedPawns[2];
	U64 m_doubledPawns[2];
	U64 m_isolatedPawns[2];
	U64 m_strongFields[2];
};


#endif
