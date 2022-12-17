/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2002-2018 Vladimir Medvedev <vrm@bk.ru> (GreKo author)
*  Copyright (C) 2018-2023 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

#ifndef MOVES_H
#define MOVES_H

#include "position.h"

struct SMove
{
    SMove() : m_mv(0), m_score(0) {}
    SMove(Move mv) : m_mv(mv), m_score(0) {}
    SMove(Move mv, EVAL score) : m_mv(mv), m_score(score) {}

    Move m_mv;
    int  m_score;
};

class MoveList
{
public:
    MoveList();

public:
    void Add(Move mv);
    void Add(FLD from, FLD to, PIECE piece);
    void Add(FLD from, FLD to, PIECE piece, PIECE captured);
    void Add(FLD from, FLD to, PIECE piece, PIECE captured, PIECE promotion);
    void Clear() { m_size = 0; }
    size_t Size() const { return m_size; }

    const SMove& operator[](size_t i) const { return m_data[i]; }
    SMove& operator[](size_t i) { return m_data[i]; }
    void Swap(size_t i, size_t j);

private:
    SMove m_data[256];
    size_t m_size;
};

void GenAllMoves(const Position& pos, MoveList& mvlist);
void GenCapturesAndPromotions(const Position& pos, MoveList& mvlist);
void AddSimpleChecks(const Position& pos, MoveList& mvlist);
void GenMovesInCheck(const Position& pos, MoveList& mvlist);

#endif
