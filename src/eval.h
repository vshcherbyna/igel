//   GreKo chess engine
//   (c) 2002-2018 Vladimir Medvedev <vrm@bk.ru>
//   http://greko.su

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
////////////////////////////////////////////////////////////////////////////////

#endif
