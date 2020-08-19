/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2002-2018 Vladimir Medvedev <vrm@bk.ru> (GreKo author)
*  Copyright (C) 2018-2020 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

#include "evaluate.h"
#include "position.h"
#include "time.h"
#include "tt.h"

#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <functional>

const int MIN_LEVEL = 0;
const int MAX_LEVEL = 20;
const int DEFAULT_LEVEL = MAX_LEVEL;
const int MEDIUM_LEVEL = MAX_LEVEL / 2;

const int MAX_PLY = 128;

const U8 TERMINATED_BY_USER		= 0x01;
const U8 TERMINATED_BY_LIMIT	= 0x02;
const U8 SEARCH_TERMINATED		= TERMINATED_BY_USER | TERMINATED_BY_LIMIT;

const U8 MODE_PLAY    = 0x04;
const U8 MODE_ANALYZE = 0x08;
const U8 MODE_SILENT  = 0x10;

#define isCheckMateScore(score)        ((score) <= -CHECKMATE_SCORE + 50|| \
                                        (score) >=  CHECKMATE_SCORE - 50)

#define MATED_IN_MAX (MAX_PLY - CHECKMATE_SCORE)

class Search
{
    friend class History;
    friend class MoveEval;

public:
    Search();
    ~Search();
    Search(const Search&) = delete;
    Search& operator=(const Search&) = delete;

public:
    uint64_t startSearch(Time time, int depth, bool ponder, bool bench = false);
    void clearHistory();
    void clearKillers();
    void clearStacks();
    void setTime(Time time) {m_time = time;}
    void setThreadCount(unsigned int threads);
    unsigned int getThreadsCount();
    void setSyzygyDepth(int depth);
    void setPonderHit();
    void startPrincipalSearch(Time time, bool ponder);
    void stopPrincipalSearch();
    void isReady();
    void setLevel(int level);
    bool setFEN(const std::string& fen);
    bool setInitialPosition();
    bool makeMove(Move mv);

private:
    void startWorkerThreads(Time time);
    void stopWorkerThreads();
    void lazySmpSearcher();
    void indicateWorkersStop();
#if defined (SYZYGY_SUPPORT)
    Move tableBaseRootSearch();
#endif
    EVAL abSearch(EVAL alpha, EVAL beta, int depth, int ply, bool isNull, bool rootNode, Move skipMove = 0);
    EVAL qSearch(EVAL alpha, EVAL beta, int ply, int depth);
    int extensionRequired(Move mv, Move lastMove, bool inCheck, int ply, bool onPV, size_t quietMoves, int cmhistory, int fmhistory);
    bool ProbeHash(TEntry & hentry);
    bool isGameOver(Position & pos, std::string & result, std::string & comment, Move & bestMove, int & legalMoves);
    void printPV(const Position& pos, int iter, int selDepth, EVAL score, const Move* pv, int pvSize, Move mv, uint64_t sumNodes, uint64_t sumHits, uint64_t nps);
    bool isDraw();

    bool checkLimits();
    void releaseHelperThreads();
    void waitUntilCompletion();

private:
    NODES m_nodes;
    NODES m_tbHits;
    NODES m_limitCheck;
    U32 m_t0;
    volatile U8 m_flags;
    int m_depth;
    int m_syzygyDepth;
    int m_selDepth;
    int m_iterPVSize;
    MoveList m_lists[MAX_PLY];
    Move m_pv[MAX_PLY][MAX_PLY];
    int m_pvSize[MAX_PLY];
    Move m_iterPV[MAX_PLY];
    Move m_killerMoves[MAX_PLY][2];
    int m_history[2][64][64];
    Move m_moveStack[MAX_PLY + 4];
    PIECE m_pieceStack[MAX_PLY + 4];
    EVAL m_evalStack[MAX_PLY + 4];
    int m_followTable[2][14][64][14][64];
    Move m_counterTable[2][14][64];
    int m_logLMRTable[64][64];
    Time m_time, m_ponderTime;
    std::unique_ptr<std::thread> m_principalThread;
    std::mutex m_readyMutex;
    std::unique_ptr<Evaluator> m_evaluator;

public:
    bool m_principalSearcher;
    Position m_position;

private:
    bool getIsLazySmpWork() {return (m_lazyDepth > 0);}
    void resetLazySmpWork() {m_lazyDepth = 0;}
    unsigned int m_thc;
    std::unique_ptr<std::thread[]> m_threads;
    std::unique_ptr<Search[]> m_threadParams;
    std::condition_variable m_lazycv;
    volatile int m_lazyDepth;
    volatile bool m_smpThreadExit;
    bool m_lazyPonder;
    static constexpr int m_lmpDepth = 8;
    static constexpr int m_lmpPruningTable[2][9] =
    {
        {  0,  3,  4,  6, 10, 14, 19, 25, 31 },
        {  0,  5,  7, 11, 17, 26, 36, 48, 63 },
    };
    static constexpr int m_cmpDepth[]        = { 3, 2           };
    static constexpr int m_cmpHistoryLimit[] = { 0, -1000       };
    static constexpr int m_fmpDepth[]        = { 3, 2           };
    static constexpr int m_fmpHistoryLimit[] = { -2000, -4000   };
    static constexpr int m_fpHistoryLimit[]  = { 12000, 6000    };
    bool m_terminateSmp;
    int m_level;
    bool m_ponderHit;
};

#endif
