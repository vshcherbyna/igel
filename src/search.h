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

#ifndef SEARCH_H
#define SEARCH_H

#include "position.h"

extern U32 g_stHard;
extern U32 g_stSoft;
extern U32 g_inc;
extern int g_sd;
extern NODES g_sn;

const int MAX_PLY = 64;

const U8 TERMINATED_BY_USER  = 0x01;
const U8 TERMINATED_BY_LIMIT = 0x02;
const U8 SEARCH_TERMINATED = TERMINATED_BY_USER | TERMINATED_BY_LIMIT;

const U8 MODE_PLAY    = 0x04;
const U8 MODE_ANALYZE = 0x08;
const U8 MODE_SILENT  = 0x10;

void ClearHash();
void ClearHistory();
void ClearKillers();
bool IsGameOver(Position& pos, string& result, string& comment);
void SetHashSize(double mb);
void SetStrength(int level);
void StartPerft(Position& pos, int depth);
Move StartSearch(const Position& pos, U8 flags);

struct HashEntry
{
    HashEntry() : m_hash(0), m_mv(0) {}
    U64  m_hash;
    Move m_mv;
    EVAL m_score;
    I8   m_depth;
    U8   m_type;
    U8   m_age;
};


#endif
