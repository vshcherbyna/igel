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
#include "time.h"
#include "tt.h"

const int MAX_PLY = 64;

const U8 TERMINATED_BY_USER  = 0x01;
const U8 TERMINATED_BY_LIMIT = 0x02;
const U8 SEARCH_TERMINATED = TERMINATED_BY_USER | TERMINATED_BY_LIMIT;

const U8 MODE_PLAY    = 0x04;
const U8 MODE_ANALYZE = 0x08;
const U8 MODE_SILENT  = 0x10;


class Search
{
public:
    Search();
    ~Search() = default;
    Search(const Search&) = delete;
    Search& operator=(const Search&) = delete;

public:
    Move StartSearch(bool principal, int depth, Time time);
    void ClearHistory();
    void ClearKillers();
    void setPosition(Position pos);

private:
    EVAL AlphaBetaRoot(EVAL alpha, EVAL beta, int depth);
    EVAL AlphaBeta(EVAL alpha, EVAL beta, int depth, int ply, bool isNull);
    EVAL AlphaBetaQ(EVAL alpha, EVAL beta, int ply, int qply);
    int Extensions(Move mv, Move lastMove, bool inCheck, int ply, bool onPV);
    Move GetNextBest(MoveList& mvlist, size_t i);
    bool IsGoodCapture(Move mv);
    bool ProbeHash(TEntry & hentry);
    void RecordHash(Move mv, EVAL score, I8 depth, int ply, U8 type);
    EVAL SEE(Move mv);
    void UpdateSortScores(MoveList& mvlist, Move hashMove, int ply);
    void UpdateSortScoresQ(MoveList& mvlist, int ply);
    bool HaveSingleMove(Position& pos, Move & bestMove);
    bool IsGameOver(Position& pos, string& result, string& comment);
    void PrintPV(const Position& pos, int iter, EVAL score, const Move* pv, int pvSize, const string& sign);
    EVAL SEE_Exchange(FLD to, COLOR side, EVAL currScore, EVAL target, U64 occ);
    Move FirstLegalMove(Position& pos);

    void ProcessInput(const string& s);
    bool CheckLimits();
    void CheckInput();

private:
    NODES m_timeCheck;
    NODES m_nodes;
    U32 m_t0;
    U8 m_flags;
    int m_depth;
    int m_iterPVSize;
    bool m_principalSearcher;
    Position m_position;
    MoveList m_lists[MAX_PLY];
    Move m_pv[MAX_PLY][MAX_PLY];
    int m_pvSize[MAX_PLY];
    Move m_iterPV[MAX_PLY];
    Move m_mateKillers[MAX_PLY];
    Move m_killers[MAX_PLY];
    int m_histTry[64][14];
    int m_histSuccess[64][14];
    Time m_time;
};

#endif
