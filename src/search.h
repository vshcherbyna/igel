/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2002-2018 Vladimir Medvedev <vrm@bk.ru> (GreKo author)
*  Copyright (C) 2018-2025 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

#include "nnue.h"
#include "position.h"
#include "thread.h"
#include "time.h"
#include "tt.h"
#include "tune.h"
#include "utils.h"

#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <functional>

const int MIN_LEVEL = 0;
const int MAX_LEVEL = 20;
const int DEFAULT_LEVEL = MAX_LEVEL;
const int MEDIUM_LEVEL = MAX_LEVEL / 2;

const U8 TERMINATED_BY_USER		= 0x01;
const U8 TERMINATED_BY_LIMIT	= 0x02;
const U8 SEARCH_TERMINATED		= TERMINATED_BY_USER | TERMINATED_BY_LIMIT;

const U8 MODE_PLAY    = 0x04;
const U8 MODE_ANALYZE = 0x08;
const U8 MODE_SILENT  = 0x10;

#define isCheckMateScore(score)        ((score) <= -CHECKMATE_SCORE + 50|| \
                                        (score) >=  CHECKMATE_SCORE - 50)

#define MATED_IN_MAX (MAX_PLY - CHECKMATE_SCORE)

struct HelperSearch;

class Search
{
    friend class History;
    friend class MoveEval;
    friend class GenWorker;
#if defined(UNIT_TEST)
    friend struct HistoryTestAccess;
#endif

public:
    Search();
    ~Search();
    Search(const Search&) = delete;
    Search& operator=(const Search&) = delete;

public:
    uint64_t startSearch(Time time, int depth, bool ponder, bool bench = false);
    void setInitial();
    void clearHistory();
    void clearKillers();
    void clearStacks();
    void initLmrTable();
    void setTime(Time time) {m_time = time;}
    void setThreadCount(unsigned int threads, bool rebuild = false);
    unsigned int getThreadsCount();
    void setSyzygyDepth(int depth);
    void setPonderHit();
    void startPrincipalSearch(Time time, bool ponder);
    void stopPrincipalSearch();
    void isReady();
    void stopAndWait();
    void setLevel(int level);
    bool setFEN(const std::string& fen);
    bool setInitialPosition();
    bool makeMove(Move mv);
    bool isGameOver(Position & pos, std::string & result, std::string & comment, Move & bestMove, int & legalMoves);
private:
    void startWorkerThreads(Time time);
    void stopWorkerThreads();
    void lazySmpSearcher();
    void indicateWorkersStop();
#if defined (SYZYGY_SUPPORT)
    Move tableBaseRootSearch(EVAL & tbScore);
#endif
    HOT_FUNCTION EVAL abSearch(EVAL alpha, EVAL beta, int depth, int ply, bool isNull, bool rootNode, bool cutNode, Move skipMove = 0);
    HOT_FUNCTION EVAL qSearch(EVAL alpha, EVAL beta, int ply, int depth, bool isNull = false);
    FORCE_INLINE int extensionRequired(bool inCheck, bool onPV, int cmhistory, int fmhistory)
    {
        if (!onPV && cmhistory >= s_histExtLimit && fmhistory >= s_histExtLimit)
            return 1;
        else if (inCheck)
            return 1;
        return 0;
    }
    bool ProbeHash(TEntry & hentry, U64 hash);
    HOT_FUNCTION int correctionValue(int ply) const;
    EVAL correctedStaticEval(EVAL eval, int ply) const;
    HOT_FUNCTION void updateCorrectionHistory(int ply, int bonus);
    void clearCorrectionHistory();

    static FORCE_INLINE int correctionIndex(U64 key) {
        return static_cast<int>((key * 0x9E3779B97F4A7C15ull) >> (64 - m_correctionHistoryBits));
    }

    static FORCE_INLINE int pawnHistoryIndex(U64 key) { // top bits: the lowest bits of the lcg-generated zobrist keys depend only on a pawn's file
        return static_cast<int>((key * 0x9E3779B97F4A7C15ull) >> (64 - m_pawnHistoryBits));
    }

    FORCE_INLINE void prefetchCorrection() {
        const COLOR side = m_position.Side();
        auto & history   = *m_correctionHistory;

        prefetch(&history.pawn[side][correctionIndex(m_position.pawnHash())]);
        prefetch(&history.minor[side][correctionIndex(m_position.minorHash())]);
        prefetch(&history.nonPawn[side][WHITE][correctionIndex(m_position.nonPawnHash(WHITE))]);
        prefetch(&history.nonPawn[side][BLACK][correctionIndex(m_position.nonPawnHash(BLACK))]);
    }
    void printPV(const Position& pos, int iter, int selDepth, EVAL score, const Move* pv, int pvSize, Move mv, uint64_t sumNodes, uint64_t sumHits, uint64_t nps);
    HOT_FUNCTION bool isDraw();

    HOT_FUNCTION bool checkLimits();
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
    MoveList m_lists[MAX_PLY];
    MoveList m_singularLists[MAX_PLY];  // excluded search runs at the parent's ply
    int m_singularPly = -1;             // every frame at that ply uses the parallel list
    int m_nmpMinPly = 0;
    Move m_pv[MAX_PLY][MAX_PLY];
    int m_pvSize[MAX_PLY];
    Move m_pvPrev[MAX_PLY][MAX_PLY];
    int m_pvSizePrev[MAX_PLY];
    Move m_killerMoves[MAX_PLY][2];
    bool m_ttPvStack[MAX_PLY];          // per-ply pv flag, carried into the transposition table
    Move m_counterTable[14][64] = {};   // refutation move of the previous [piece][to]
    int16_t m_history[2][64][64][4];
    static constexpr int m_pawnHistoryBits = 11;
    struct PawnHistoryTable {
        int16_t entry[1 << m_pawnHistoryBits][14][64];
    };
    std::unique_ptr<PawnHistoryTable> m_pawnHistory;
    int16_t * m_quietHistoryStack[MAX_PLY + 4] = {};
    int16_t m_captureHistory[14][64][14];   // [piece][to][captured] of captures and promotions
    Move m_moveStack[MAX_PLY + 4];
    PIECE m_pieceStack[MAX_PLY + 4];
    EVAL m_evalStack[MAX_PLY + 4];      // corrected, what search works with
    EVAL m_rawEvalStack[MAX_PLY + 4];   // uncorrected, what a null move child derives its own from
    int16_t m_followTable[2][14][64][14][64];
    static constexpr int m_lmrScale = 1024;
    int m_logLMRTable[64][64];
    int m_lmrTableBase = -1;
    Time m_time, m_ponderTime;
    std::unique_ptr<SearchThread> m_principalThread;
    std::mutex m_readyMutex;
    std::unique_ptr<Evaluator> m_evaluator;

    static constexpr int m_correctionHistoryBits = 16;
    static constexpr int m_correctionHistorySize = 1 << m_correctionHistoryBits;
    static constexpr int m_correctionHistoryLimit = 1024;

    static constexpr int s_noPrevMoveBias     = 150033;
    static constexpr int s_correctionGrain    = 294912;

    TUNABLE(s_histBonusQuad,        "HistBonusQuad",             64,     16,    192,    6.00, 0.0020);
    TUNABLE(s_histBonusBase,        "HistBonusBase",              0,    -16,    128,    4.00, 0.0020);
    TUNABLE(s_histBonusMax,         "HistBonusMax",             400,    128,    512,   25.00, 0.0020);
    TUNABLE(s_histMalusQuad,        "HistMalusQuad",             64,     16,    192,    6.00, 0.0020);
    TUNABLE(s_histMalusBase,        "HistMalusBase",              0,    -16,    128,    4.00, 0.0020);
    TUNABLE(s_histMalusMax,         "HistMalusMax",             400,    128,    512,   25.00, 0.0020);
    TUNABLE(s_staticHistMul,        "StaticHistMul",             16,      0,     64,    2.00, 0.0020);
    TUNABLE(s_staticHistClamp,      "StaticHistClamp",           32,      4,     96,    3.00, 0.0020);
    TUNABLE(s_pawnMalusMul,         "PawnMalusMul",             256,      0,   1024,   32.00, 0.0020);
    TUNABLE(s_pawnOrderWeight,      "PawnOrderWeight",         1024,      0,   2048,   64.00, 0.0020);
    TUNABLE(s_captOrderWeight,      "CaptOrderWeight",           64,      0,    192,    8.00, 0.0020);
    TUNABLE(s_orderCmWeight,        "OrderCmWeight",           1024,    256,   2048,   64.00, 0.0020);
    TUNABLE(s_orderFmWeight,        "OrderFmWeight",           1024,    256,   2048,   64.00, 0.0020);
    TUNABLE(s_histExtLimit,         "HistExtLimit",           10000,   4000,  16000,  500.00, 0.0020);
    TUNABLE(s_lmrHistScale,         "LmrHistScale",             755,      0,   2048,   50.00, 0.0020);
    TUNABLE(s_lmrHistClamp,         "LmrHistClamp",            1139,    256,   3072,   80.00, 0.0020);
    TUNABLE(s_cmpLimit,             "CmpLimit",               -1129,  -6000,   2000,  250.00, 0.0020);
    TUNABLE(s_cmpLimitImp,          "CmpLimitImp",            -1151,  -6000,   2000,  250.00, 0.0020);
    TUNABLE(s_fmpLimit,             "FmpLimit",               -2665,  -8000,   2000,  250.00, 0.0020);
    TUNABLE(s_fmpLimitImp,          "FmpLimitImp",            -4367, -12000,      0,  300.00, 0.0020);

    TUNABLE(s_razoringMargin,       "RazoringMargin",           130,     50,    400,    9.60, 0.0020);
    TUNABLE(s_staticNmpMargin,      "StaticNmpMargin",           42,     15,    150,    4.80, 0.0020);
    TUNABLE(s_nmpEvalDivisor,       "NmpEvalDivisor",            92,     40,    300,    7.20, 0.0020);
    TUNABLE(s_futilityMargin,       "FutilityMargin",            85,     30,    250,    7.20, 0.0020);
    TUNABLE(s_futilityBase,         "FutilityBase",              11,   -100,    300,   12.00, 0.0020);
    TUNABLE(s_singularBetaScale,    "SingularBetaScale",        112,     25,    400,   12.00, 0.0020);
    TUNABLE(s_singularDoubleMargin, "SingularDoubleMargin",      53,      0,    200,    6.00, 0.0020);
    TUNABLE(s_deeperMargin,         "DeeperMargin",              45,      0,    200,    6.00, 0.0020);
    TUNABLE(s_deeperDepthScale,     "DeeperDepthScale",         145,      0,    800,   24.00, 0.0020);
    TUNABLE(s_shallowerMargin,      "ShallowerMargin",           13,      0,     60,    2.40, 0.0020);
    TUNABLE(s_lmrEvalGapMin,        "LmrEvalGapMin",             69,      0,    256,    9.60, 0.0020);
    TUNABLE(s_lmrEvalGapMax,        "LmrEvalGapMax",            105,      0,    256,    9.60, 0.0020);
    TUNABLE(s_lmrEvalGapScale,      "LmrEvalGapScale",          960,      0,   4096,  100.00, 0.0020);
    TUNABLE(s_aspWindow,            "AspWindow",                  8,      2,     40,    1.20, 0.0020);

    TUNABLE(s_lmrBase,              "LmrBase",                  942,      0,   1500,   50.00, 0.0020);
    TUNABLE(s_lmrCutNode,           "LmrCutNode",              1152,      0,   3072,  100.00, 0.0020);
    TUNABLE(s_lmrTtCapture,         "LmrTtCapture",            1212,      0,   3072,  100.00, 0.0020);
    TUNABLE(s_lmrTtPv,              "LmrTtPv",                  292,      0,   2048,   64.00, 0.0020);
    TUNABLE(s_lmrImproving,         "LmrImproving",             368,      0,   2048,   64.00, 0.0020);
    TUNABLE(s_lmrCheck,             "LmrCheck",                 488,      0,   3072,  100.00, 0.0020);
    TUNABLE(s_lmrKiller,            "LmrKiller",                788,      0,   3072,  100.00, 0.0020);
    TUNABLE(s_lmrNoisyBase,         "LmrNoisyBase",             852,      0,   3072,  100.00, 0.0020);
    TUNABLE(s_lmrGoodCapture,       "LmrGoodCapture",           431,      0,   2048,   64.00, 0.0020);
    TUNABLE(s_lmrVictimScale,       "LmrVictimScale",          1240,      0,   3072,  100.00, 0.0020);
    TUNABLE(s_lmrCaptHistScale,     "LmrCaptHistScale",        1216,      0,   4096,  100.00, 0.0020);
    TUNABLE(s_lmpScale,             "LmpScale",                1024,    704,   1536,   64.00, 0.0020);
    TUNABLE(s_lmpScaleImp,          "LmpScaleImp",             1024,    704,   1536,   64.00, 0.0020);
    TUNABLE(s_seeQuietMargin,       "SeeQuietMargin",           -50,   -200,    -10,    4.00, 0.0020);
    TUNABLE(s_seeNoisyMargin,       "SeeNoisyMargin",           -12,    -40,     -2,    1.00, 0.0020);

    TUNABLE(s_nmpBase,              "NmpBase",                 3840,   2304,   5376,  192.00, 0.0020);
    TUNABLE(s_nmpDepthMul,          "NmpDepthMul",              128,     48,    288,   12.00, 0.0020);

    TUNABLE(s_pawnWeight,           "CorrPawnWeight",          8492,      0,  24000,  425.00, 0.0020);
    TUNABLE(s_minorWeight,          "CorrMinorWeight",         2489,      0,  24000,  125.00, 0.0020);
    TUNABLE(s_nonPawnWhiteWeight,   "CorrNonPawnWhiteWeight",  6319,      0,  24000,  316.00, 0.0020);
    TUNABLE(s_nonPawnBlackWeight,   "CorrNonPawnBlackWeight",  9512,      0,  24000,  476.00, 0.0020);
    TUNABLE(s_cont2Weight,          "CorrCont2Weight",         9242,      0,  24000,  462.00, 0.0020);
    TUNABLE(s_cont4Weight,          "CorrCont4Weight",         2357,      0,  24000,  118.00, 0.0020);
    TUNABLE(s_corrBonusBestMove,    "CorrBonusBestMove",        104,      8,    256,    8.00, 0.0020);
    TUNABLE(s_corrBonusFailLow,     "CorrBonusFailLow",         128,      8,    256,    8.00, 0.0020);
    TUNABLE(s_corrBonusMax,         "CorrBonusMax",             262,     16,   1024,   13.00, 0.0020);

    struct CorrectionHistoryTable {
        I16 pawn[COLORS][m_correctionHistorySize];
        I16 minor[COLORS][m_correctionHistorySize];
        I16 nonPawn[COLORS][COLORS][m_correctionHistorySize];
        I16 continuation[14][64][14][64];
    };

    std::unique_ptr<CorrectionHistoryTable> m_correctionHistory;

public:
    bool m_principalSearcher;
    Position m_position;

private:
    bool getIsLazySmpWork() {return (m_lazyDepth > 0);}
    void resetLazySmpWork() {m_lazyDepth = 0;}
    unsigned int m_thc;
    std::unique_ptr<SearchThread[]> m_threads;
    HelperSearch * m_threadParams;
    std::condition_variable m_lazycv;
    volatile int m_lazyDepth;
    volatile bool m_smpThreadExit;
    volatile bool m_stopRequested;
    bool m_lazyPonder;
    static constexpr int m_lmpDepth = 8;
    static constexpr int m_lmpPruningTable[2][9] =
    {
        {  0,  1,  2,  3,  6, 11, 16, 22, 30 },
        {  0,  5,  7, 10, 15, 24, 33, 44, 58 },
    };
    static constexpr int m_cmpDepth[]        = { 3, 2           };
    static constexpr int m_fmpDepth[]        = { 3, 2           };
    static constexpr int m_fpHistoryLimit[]  = { 12308, 5432    };
    bool m_terminateSmp;
    int m_level;
    bool m_ponderHit;
    EVAL m_score;
    Move m_best,
         m_ponder;
    NODES m_rootMoveNodes[64][64];
};

struct HelperSearch : Search
{
    char m_gap[2 * 1024 * 1024];
};

#endif
