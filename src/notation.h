//   GreKo chess engine
//   (c) 2002-2018 Vladimir Medvedev <vrm@bk.ru>
//   http://greko.su

#ifndef NOTATION_H
#define NOTATION_H

#include "moves.h"

bool   CanBeMove(const string& s);
string FldToStr(FLD f);
string MoveToStrLong(Move mv);
string MoveToStrShort(Move mv, Position& pos, const MoveList& mvlist);
FLD    StrToFld(const string& s);
Move   StrToMove(const string& s, Position& pos);

#endif
