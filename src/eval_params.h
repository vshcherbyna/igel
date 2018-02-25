//   GreKo chess engine
//   (c) 2002-2018 Vladimir Medvedev <vrm@bk.ru>
//   http://greko.su

#ifndef EVAL_PARAMS_H
#define EVAL_PARAMS_H

#include "types.h"

extern vector<int> W;

struct Line
{
	Line() : name(""), start(0), len(0) {}
	Line(const string& n, int l) : name(n), start(0), len(l) {}

	string name;
	int start;
	int len;
};

enum Param
{
	Mid_Pawn,
	Mid_Knight,
	Mid_Bishop,
	Mid_Rook,
	Mid_Queen,
	Mid_King,
	Mid_PawnPassedBlocked,
	Mid_PawnPassedFree,
	Mid_PawnDoubled,
	Mid_PawnIsolated,
	Mid_KnightStrong,
	Mid_BishopPair,
	Mid_BishopStrong,
	Mid_BishopMobility,
	Mid_RookMobility,
	Mid_RookOpen,
	Mid_Rook7th,
	Mid_Queen7th,
	Mid_QueenKingDist,
	Mid_KingPawnShield,
	Mid_KingPassedDist,
	Mid_AttackKingZone,
	Mid_AttackStronger,

	End_Pawn,
	End_Knight,
	End_Bishop,
	End_Rook,
	End_Queen,
	End_King,
	End_PawnPassedBlocked,
	End_PawnPassedFree,
	End_PawnDoubled,
	End_PawnIsolated,
	End_KnightStrong,
	End_BishopPair,
	End_BishopStrong,
	End_BishopMobility,
	End_RookMobility,
	End_RookOpen,
	End_Rook7th,
	End_Queen7th,
	End_QueenKingDist,
	End_KingPawnShield,
	End_KingPassedDist,
	End_AttackKingZone,
	End_AttackStronger,

	NUM_LINES
};

extern Line lines[NUM_LINES];
extern int NUM_PARAMS;

void InitParamLines();
std::string ParamNumberToName(size_t n);

void SetDefaultValues(vector<int>& x);
void SetMaterialOnlyValues(vector<int>& x);

bool ReadParamsFromFile(vector<int>& x, const std::string& filename);
void WriteParamsToFile(const vector<int>& x, const std::string& filename);

#endif
