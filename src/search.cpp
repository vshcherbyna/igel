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

#include "eval.h"
#include "moves.h"
#include "notation.h"
#include "search.h"
#include "utils.h"
#include "fathom/tbprobe.h"
#include "history.h"
#include "moveeval.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <utility>

const U8 HASH_ALPHA = 0;
const U8 HASH_EXACT = 1;
const U8 HASH_BETA = 2;

/*static */constexpr int Search::m_lmpDepth;
/*static */constexpr int Search::m_lmpPruningTable[2][9];
/*static */constexpr int Search::m_cmpDepth[2];
/*static */constexpr int Search::m_cmpHistoryLimit[2];
/*static */constexpr int Search::m_fmpDepth[2];
/*static */constexpr int Search::m_fmpHistoryLimit[2];
/*static */constexpr int Search::m_fpHistoryLimit[2];
/*static */constexpr int Search::m_skipSize[16];
/*static */constexpr int Search::m_skipDepths[16];
/*static */constexpr int Search::m_SMPCycles;

extern FILE * g_log;

Search::Search() :
    m_nodes(0),
    m_tbHits(0),
    m_limitCheck(1023),
    m_t0(0),
    m_flags(0),
    m_depth(0),
    m_syzygyDepth(0),
    m_selDepth(0),
    m_iterPVSize(0),
    m_index(0),
    m_principalSearcher(false),
    m_thc(0),
    m_threads(nullptr),
    m_threadParams(nullptr),
    m_lazyDepth(0),
    m_smpThreadExit(false),
    m_lazyPonder(false),
    m_terminateSmp(false),
    m_waitStarted(false),
    m_level(DEFAULT_LEVEL)
{
    m_evaluator.reset(new Evaluator);
    memset(&m_logLMRTable, 0, sizeof(m_logLMRTable));

    for (int depth = 1; depth < 64; ++depth)
        for (int moves = 1; moves < 64; ++moves)
            m_logLMRTable[depth][moves] = 0.75 + log(depth) * log(moves) / 2.25;
}

Search::~Search()
{
    releaseHelperThreads();
}

bool Search::checkLimits()
{
    if (m_flags & SEARCH_TERMINATED)
        return true;

    if (m_smpThreadExit) {
        m_flags |= TERMINATED_BY_LIMIT;
        return true;
    }

    if (m_time.getTimeMode() == Time::TimeControl::NodesLimit) {
        if (m_nodes >= m_time.getNodesLimit())
            m_flags |= TERMINATED_BY_LIMIT;

        return (m_flags & SEARCH_TERMINATED);
    }

    ++m_limitCheck;

    if (!(m_limitCheck &= 1023)) {

        U32 dt = GetProcTime() - m_t0;
        if (m_flags & MODE_PLAY)
        {
            if (m_time.getTimeMode() == Time::TimeControl::TimeLimit && dt >= m_time.getHardLimit())
            {
                m_flags |= TERMINATED_BY_LIMIT;

                if (g_log) {
                    stringstream ss;
                    ss << "Search stopped by stHard, dt = " << dt;
                    Log(ss.str());
                }
            }
        }
    }

    return (m_flags & SEARCH_TERMINATED);
}

void Search::setPosition(Position & pos)
{
    m_position = pos;
}

EVAL Search::abSearch(EVAL alpha, EVAL beta, int depth, int ply, bool isNull, bool pruneMoves, bool rootNode/*, Move skipMove = 0*/)
{
    m_pvSize[ply] = 0;
    m_selDepth = std::max(ply, m_selDepth);

    //
    //   qsearch
    //

    if (depth <= 0 && m_level > MEDIUM_LEVEL)
        return qSearch(alpha, beta, ply);

    ++m_nodes;

    if (!rootNode) {

        if (checkLimits())
            return DRAW_SCORE;

        if ((ply > MAX_PLY - 2) || m_position.Repetitions() >= 2)
            return ((ply > MAX_PLY - 2) && !m_position.InCheck()) ? m_evaluator->evaluate(m_position) : DRAW_SCORE;

        //
        // mate distance pruning
        //

        auto rAlpha = alpha > -CHECKMATE_SCORE + ply ? alpha : -CHECKMATE_SCORE + ply;
        auto rBeta = beta < CHECKMATE_SCORE - ply - 1 ? beta : CHECKMATE_SCORE - ply - 1;

        if (rAlpha >= rBeta)
            return rAlpha;
    }

    //
    //   probe hash
    //

    Move hashMove{};
    TEntry hEntry;
    auto onPV  = (beta - alpha > 1);
    auto ttHit = false;

    if (ProbeHash(hEntry)) {
        ttHit = hEntry.type() == HASH_EXACT;
        if (hEntry.depth() >= depth && (depth == 0 || !onPV)) {
            EVAL score = hEntry.score();
            if (score > CHECKMATE_SCORE - 50 && score <= CHECKMATE_SCORE)
                score -= ply;
            if (score < -CHECKMATE_SCORE + 50 && score >= -CHECKMATE_SCORE)
                score += ply;

            if (hEntry.type() == HASH_EXACT
                || (hEntry.type() == HASH_BETA && score >= beta)
                || (hEntry.type() == HASH_ALPHA && score <= alpha))
                return score;
        }

        hashMove = hEntry.move();
    }

    //
    //  tablebase probe
    //

    if (!rootNode && TB_LARGEST && depth >= 2 && !m_position.Fifty() && !(m_position.CanCastle(m_position.Side(), KINGSIDE) || m_position.CanCastle(m_position.Side(), QUEENSIDE))) {
        auto pieces = countBits(m_position.BitsAll());

        if ((pieces < TB_LARGEST) || (pieces == TB_LARGEST && depth >= m_syzygyDepth)) {
            EVAL score;
            U8 type;
            FLD ep = m_position.EP();

            //
            //  different board representation
            //

            if (ep == NF)
                ep = 0;
            else
                ep = abs(int(ep) - 63);

            auto probe = tb_probe_wdl(m_position.BitsAll(WHITE), m_position.BitsAll(BLACK),
                m_position.Bits(KW) | m_position.Bits(KB),
                m_position.Bits(QW) | m_position.Bits(QB),
                m_position.Bits(RW) | m_position.Bits(RB),
                m_position.Bits(BW) | m_position.Bits(BB),
                m_position.Bits(NW) | m_position.Bits(NB),
                m_position.Bits(PW) | m_position.Bits(PB),
                0,
                0,
                ep,
                m_position.Side() == WHITE);

            if (probe != TB_RESULT_FAILED) {
                m_tbHits++;
                switch (probe)
                {
                case TB_WIN:
                    score = TBBASE_SCORE - MAX_PLY - ply;
                    type = HASH_BETA;
                    break;
                case TB_LOSS:
                    score = -TBBASE_SCORE + MAX_PLY + ply;
                    type = HASH_ALPHA;
                    break;
                default:
                    score = 0;
                    type = HASH_EXACT;
                    break;
                }

                //
                //  make sure we store result of tbprobe in tt with high depth
                //

                if ((type == HASH_EXACT) || (type == HASH_ALPHA ? (score <= alpha) : (score >= beta))) {
                    TTable::instance().record(0, score, MAX_PLY, 0, type, m_position.Hash());
                    return score;
                }
            }
        }
    }

    EVAL staticEval;
    auto inCheck = m_position.InCheck();

    if (inCheck)
        staticEval = -CHECKMATE_SCORE + ply;
    else
        staticEval = m_evaluator->evaluate(m_position);

    m_evalStack[ply] = staticEval;

    if (pruneMoves && !isCheckMateScore(beta) && !inCheck) {

        //
        //   razoring
        //

        if (!onPV && depth <= 1 && staticEval + 325 < alpha)
            return qSearch(alpha, beta, ply);

        //
        //  static null move pruning
        //

        if (!onPV && depth <= 4 && staticEval - 85 * depth > beta)
            return staticEval;

        //
        //   null move
        //

        bool nonPawnMaterial = m_position.NonPawnMaterial();

        static const EVAL MARGIN[4] = { 0, 50, 350, 550 };

        if (nonPawnMaterial && !onPV && depth >= 1 && depth <= 3) {
            if (staticEval <= alpha - MARGIN[depth])
                return qSearch(alpha, beta, ply);
            if (staticEval >= beta + MARGIN[depth])
                return beta;
        }

        if (!isNull && !onPV && depth >= 2 && nonPawnMaterial && staticEval >= beta && !ttHit) {
            int R = 4 + depth / 6 + min(3, (staticEval - beta) / 200);

            m_position.MakeNullMove();
            m_moveStack[ply] = 0;
            EVAL nullScore = -abSearch(-beta, -beta + 1, depth - R, ply + 1, true, false, false);
            m_position.UnmakeNullMove();

            if (nullScore >= beta)
                return isCheckMateScore(nullScore) ? beta : nullScore;
        }

        //
        //  probcut
        //

        if (!onPV && depth >= 5 && abs(beta) < (CHECKMATE_SCORE + 50)) {
            auto betaCut = beta + 100;
            MoveList captureMoves;
            GenCapturesAndPromotions(m_position, captureMoves);
            auto captureMovesSize = captureMoves.Size();

            for (size_t i = 0; i < captureMovesSize; ++i)
            {
                if (MoveEval::SEE(this, captureMoves[i].m_mv) < betaCut - staticEval)
                    continue;

                if (m_position.MakeMove(captureMoves[i].m_mv)) {
                    auto score = -abSearch(-betaCut, -betaCut + 1, depth - 4, ply + 1, isNull, true, false);
                    m_position.UnmakeMove();

                    if (score >= betaCut)
                        return score;
                }
            }
        }
    }

    //
    //   iid
    //

    if (onPV && hashMove == 0 && depth > 4 && m_level == MAX_LEVEL)
    {
        abSearch(alpha, beta, depth - 4, ply, isNull, false, false);
        if (m_pvSize[ply] > 0)
            hashMove = m_pv[ply][0];
    }

    int legalMoves = 0;
    Move bestMove = hashMove;
    EVAL bestScore = -CHECKMATE_SCORE + ply;
    U8 type = HASH_ALPHA;

    MoveList& mvlist = m_lists[ply];
    if (inCheck)
        GenMovesInCheck(m_position, mvlist);
    else
        GenAllMoves(m_position, mvlist);

    MoveEval::sortMoves(this, mvlist, hashMove, ply);

    auto mvSize = mvlist.Size();
    std::vector<Move> quietMoves;
    auto improving = !inCheck && ply >= 2 && staticEval > m_evalStack[ply - 2];
    m_killerMoves[ply + 1][0] = m_killerMoves[ply + 1][1] = 0;
    auto quietsTried = 0;
    auto skipQuiets = false;

    for (size_t i = 0; i < mvSize; ++i) {

        Move mv = MoveEval::getNextBest(mvlist, i);
        auto quietMove = !MoveEval::isTacticalMove(mv);
        History::HistoryHeuristics history{};

        if (!rootNode && bestScore > MATED_IN_MAX) {

            if (quietMove) {
                if (skipQuiets)
                    continue;

                History::fetchHistory(this, mv, ply, history);

                auto futilityMargin = staticEval + 90 * depth;

                if (futilityMargin <= alpha
                    && depth <= 8
                    && history.history + history.cmhistory + history.fmhistory < m_fpHistoryLimit[improving])
                    skipQuiets = true;

                if (depth <= m_lmpDepth && quietsTried >= m_lmpPruningTable[improving][depth])
                    skipQuiets = true;

                if (depth <= m_cmpDepth[improving] && history.cmhistory < m_cmpHistoryLimit[improving])
                    continue;

                if (depth <= m_fmpDepth[improving] && history.fmhistory < m_fmpHistoryLimit[improving])
                    continue;
            }

            if (depth <= 8 && !inCheck) {

                int seeMargin[2];

                static const int SEEQuietMargin = -80;
                static const int SEENoisyMargin = -18;

                seeMargin[0] = SEENoisyMargin * depth * depth;
                seeMargin[1] = SEEQuietMargin * depth;

                if (MoveEval::SEE(this, mv) < seeMargin[quietMove])
                    continue;
            }
        }

        int newDepth = depth - 1;

        /*
        //
        //  singular extensions
        //

        if (depth >= 8 && !skipMove && hashMove == mv && !rootNode && bestMove && hEntry.type() == HASH_BETA && hEntry.depth() >= depth - 3) {
            auto betaCut = hEntry.score() - depth;
            auto score = abSearch(betaCut - 1, betaCut, depth / 2, ply + 1, true, mv);
            if (score < betaCut)
                newDepth++;
        }
        */

        auto lastMove = m_position.LastMove();

        if (m_position.MakeMove(mv))
        {
            ++legalMoves;

            if (quietMove) {
                ++quietsTried;
                quietMoves.emplace_back(mv);
            }

            m_moveStack[ply] = mv;
            m_pieceStack[ply] = mv.Piece();

            //
            //   extensions
            //

            newDepth += extensionRequired(mv, lastMove, inCheck, ply, onPV, quietMoves.size(), history.cmhistory, history.fmhistory);

            EVAL e;
            if (legalMoves == 1)
                e = -abSearch(-beta, -alpha, newDepth, ply + 1, false, true, false);
            else
            {
                //
                //   lmr
                //

                int reduction = 0;

                if (depth >= 3 && quietMove) {
                    reduction = m_logLMRTable[std::min(depth, 63)][std::min(legalMoves, 63)];

                    reduction -= mv == m_killerMoves[ply][0]
                        || mv == m_killerMoves[ply][1];

                    reduction -= std::max(-2, std::min(2, (history.history + history.cmhistory + history.fmhistory) / 5000));

                    if (reduction >= newDepth)
                        reduction = newDepth - 1;
                    else if (reduction < 0)
                        reduction = 0;
                }

                e = -abSearch(-alpha - 1, -alpha, newDepth - reduction, ply + 1, false, true, false);

                if (e > alpha && reduction > 0)
                    e = -abSearch(-alpha - 1, -alpha, newDepth, ply + 1, false, true, false);
                if (e > alpha && e < beta)
                    e = -abSearch(-beta, -alpha, newDepth, ply + 1, false, true, false);
            }
            m_position.UnmakeMove();

            if (m_flags & SEARCH_TERMINATED)
                return DRAW_SCORE;

            if (e > bestScore) {
                bestScore = e;
                if (e > alpha) {
                    alpha = e;
                    bestMove = mv;
                    type = HASH_EXACT;

                    m_pv[ply][0] = mv;
                    memcpy(m_pv[ply] + 1, m_pv[ply + 1], m_pvSize[ply + 1] * sizeof(Move));
                    m_pvSize[ply] = 1 + m_pvSize[ply + 1];

                    if (alpha >= beta) {
                        type = HASH_BETA;
                        if (!MoveEval::isTacticalMove(mv)) {
                            History::updateHistory(this, quietMoves, ply, depth * depth);
                            History::setKillerMove(this, mv, ply);
                        }
                        break;
                    }
                }
            }
        }
    }

    if (legalMoves == 0) {
        if (inCheck)
            bestScore = -CHECKMATE_SCORE + ply;
        else
            bestScore = DRAW_SCORE;
    }
    else
    {
        if (m_position.Fifty() >= 100)
            bestScore = DRAW_SCORE;
    }

    TTable::instance().record(bestMove, bestScore, depth, ply, type, m_position.Hash());
    return bestScore;
}

EVAL Search::qSearch(EVAL alpha, EVAL beta, int ply)
{
    m_pvSize[ply] = 0;
    m_selDepth = std::max(ply, m_selDepth);
    ++m_nodes;

    if (checkLimits())
        return DRAW_SCORE;

    if ((ply > MAX_PLY - 2) || m_position.Repetitions() >= 2)
        return ((ply > MAX_PLY - 2) && !m_position.InCheck()) ? m_evaluator->evaluate(m_position) : DRAW_SCORE;

    Move hashMove{};
    TEntry hEntry;
    auto ttHit = false;
    EVAL ttScore;

    if (ProbeHash(hEntry)) {
        ttScore = hEntry.score();
        ttHit = hEntry.type() == HASH_EXACT;
        if (ttScore > CHECKMATE_SCORE - 50 && ttScore <= CHECKMATE_SCORE)
            ttScore -= ply;
        if (ttScore < -CHECKMATE_SCORE + 50 && ttScore >= -CHECKMATE_SCORE)
            ttScore += ply;

        auto onPV = (beta - alpha) > 1;

        if (!onPV && (hEntry.type() == HASH_EXACT
            || (hEntry.type() == HASH_BETA && ttScore >= beta)
            || (hEntry.type() == HASH_ALPHA && ttScore <= alpha)))
            return ttScore;

        hashMove = hEntry.move();
    }

    auto inCheck = m_position.InCheck();
    EVAL bestScore;

    if (inCheck)
    {
        bestScore = -CHECKMATE_SCORE + ply;
    }
    else
    {
        bestScore = m_evaluator->evaluate(m_position);

        if (ttHit)
        {
            if ((hEntry.type() == HASH_BETA && ttScore > bestScore) ||
                (hEntry.type() == HASH_ALPHA && ttScore < bestScore) ||
                (hEntry.type() == HASH_EXACT))
                bestScore = ttScore;
        }

        if (bestScore >= beta)
            return bestScore;

        if (alpha < bestScore)
            alpha = bestScore;
    }

    MoveList& mvlist = m_lists[ply];
    if (inCheck)
        GenMovesInCheck(m_position, mvlist);
    else
        GenCapturesAndPromotions(m_position, mvlist);

    MoveEval::sortMoves(this, mvlist, hashMove, ply);

    int legalMoves = 0;
    auto mvSize = mvlist.Size();
    Move bestMove = ttHit ? hEntry.move() : Move{};

    for (size_t i = 0; i < mvSize; ++i) {
        Move mv = MoveEval::getNextBest(mvlist, i);

        if (!inCheck && MoveEval::SEE(this, mv) < 0)
            continue;

        if (m_position.MakeMove(mv)) {
            ++legalMoves;

            m_moveStack[ply] = mv;
            m_pieceStack[ply] = mv.Piece();

            EVAL e = -qSearch(-beta, -alpha, ply + 1);
            m_position.UnmakeMove();

            if (m_flags & SEARCH_TERMINATED)
                return DRAW_SCORE;

            if (e > bestScore) {
                bestScore = e;
                if (e > alpha) {
                    alpha = e;
                    bestMove = mv;
                    m_pv[ply][0] = mv;
                    memcpy(m_pv[ply] + 1, m_pv[ply + 1], m_pvSize[ply + 1] * sizeof(Move));
                    m_pvSize[ply] = 1 + m_pvSize[ply + 1];
                }
            }

            if (alpha >= beta) {
                break;
            }
        }
    }

    if (!legalMoves && inCheck)
        bestScore = -CHECKMATE_SCORE + ply;

    assert((m_position.Fifty() >= 100) == false); // in qs promotions and captures discard the rule
    return bestScore;
}

void Search::clearHistory()
{
    memset(m_history, 0, sizeof(m_history));

    for (unsigned int i = 0; i < m_thc; ++i)
        memset(m_threadParams[i].m_history, 0, sizeof(m_history));
}

void Search::clearKillers()
{
    memset(m_killerMoves, 0, sizeof(m_killerMoves));

    for (unsigned int i = 0; i < m_thc; ++i)
        memset(m_threadParams[i].m_killerMoves, 0, sizeof(m_killerMoves));
}

void Search::clearStacks()
{
    memset(m_moveStack, 0, sizeof(m_moveStack));
    memset(m_pieceStack, 0, sizeof(m_pieceStack));
    memset(m_followTable, 0, sizeof(m_followTable));
    memset(m_counterTable, 0, sizeof(m_counterTable));
    memset(m_evalStack, 0, sizeof(m_evalStack));
    memset(m_pvSize, 0, sizeof(m_pvSize));

    for (unsigned int i = 0; i < m_thc; ++i) {
        memset(m_threadParams[i].m_moveStack, 0, sizeof(m_moveStack));
        memset(m_threadParams[i].m_pieceStack, 0, sizeof(m_pieceStack));
        memset(m_threadParams[i].m_followTable, 0, sizeof(m_followTable));
        memset(m_threadParams[i].m_counterTable, 0, sizeof(m_counterTable));
        memset(m_threadParams[i].m_evalStack, 0, sizeof(m_evalStack));
        memset(m_threadParams[i].m_pvSize, 0, sizeof(m_threadParams[i].m_pvSize));
    }
}

int Search::extensionRequired(Move mv, Move lastMove, bool inCheck, int ply, bool onPV, size_t quietMoves, int cmhistory, int fmhistory)
{
    if (quietMoves <= 4 && cmhistory >= 10000 && fmhistory >= 10000)
        return 1;
    else if (inCheck)
        return 1;
    else if (ply < 2 * m_depth)
    {
        if (onPV && lastMove && mv.To() == lastMove.To() && lastMove.Captured())
            return 1;
    }
    return 0;
}

Move Search::forceFetchPonder(Position & pos, const Move & bestMove)
{
    Move ponder{};
    auto legalMoves = 0;

    if (pos.MakeMove(bestMove)) {
        MoveList mvlist{};
        GenAllMoves(pos, mvlist);

        for (size_t i = 0; i < mvlist.Size(); ++i) {
            ponder = mvlist[i].m_mv;
            if (pos.MakeMove(ponder)) {
                ++legalMoves;
                pos.UnmakeMove();

                if (legalMoves > 1)
                    break;
            }
        }

        pos.UnmakeMove();
    }

    return legalMoves ? ponder : Move{};
}

bool Search::isGameOver(Position & pos, string & result, string & comment, Move & bestMove, int & legalMoves)
{
    MoveList mvlist;
    GenAllMoves(pos, mvlist);
    legalMoves = 0;
    auto mvSize = mvlist.Size();
    Move mv{};
    Move lastLegal{};

    for (size_t i = 0; i < mvSize; ++i) {
        mv = mvlist[i].m_mv;
        if (pos.MakeMove(mv)) {
            ++legalMoves;
            lastLegal = mv;
            bestMove  = lastLegal;
            pos.UnmakeMove();

            if (legalMoves > 1)
                break;
        }
    }

    if (pos.Count(PW) == 0 && pos.Count(PB) == 0) {
        if (pos.MatIndex(WHITE) < 5 && pos.MatIndex(BLACK) < 5)
        {
            result = "1/2-1/2";
            comment = "{Insufficient material}";
            return true;
        }
    }

    if (legalMoves == 1) {
        bestMove = lastLegal;
    }
    else if (legalMoves == 0) {
        if (pos.InCheck())
        {
            if (pos.Side() == WHITE) {
                result = "0-1";
                comment = "{Black mates}";
            }
            else
            {
                result = "1-0";
                comment = "{White mates}";
            }
        }
        else
        {
            result = "1/2-1/2";
            comment = "{Stalemate}";
        }

        return true;
    }

    if (pos.Fifty() >= 100) {
        result = "1/2-1/2";
        comment = "{Fifty moves rule}";
        return true;
    }

    if (pos.Repetitions() >= 3) {
        result = "1/2-1/2";
        comment = "{Threefold repetition}";
        return true;
    }

    return false;
}

void Search::printPV(const Position& pos, int iter, int selDepth, EVAL score, const Move* pv, int pvSize, Move mv, uint64_t sumNodes, uint64_t sumHits, uint64_t nps)
{
    auto dt = GetProcTime() - m_t0;

    cout << "info depth " << iter << " seldepth " << selDepth;

    if (abs(score) >= (CHECKMATE_SCORE - MAX_PLY))
        cout << " score mate" << ((score >= 0) ? " " : " -") << ((CHECKMATE_SCORE - abs(score)) / 2) + 1;
    else
        cout << " score cp " << score;

    cout << " time " << dt;
    cout << " nodes " << sumNodes;
    cout << " tbhits " << sumHits;

    if (nps)
        cout << " nps " << nps;

    cout << " pv";

    if (pvSize > 0) {
        for (int i = 0; i < pvSize; ++i)
            cout << " " << MoveToStrLong(pv[i]);
    }
    else
        cout << " " << MoveToStrLong(mv);

    cout << endl;
}

bool Search::ProbeHash(TEntry & hentry)
{
    U64 hash = m_position.Hash();
    TEntry * nEntry = TTable::instance().retrieve(hash);

    if (!nEntry)
        return false;

    hentry = *nEntry;

    return ((hentry.m_key ^ hentry.m_data) == hash);
}

Move Search::tableBaseRootSearch()
{
    if (!TB_LARGEST || countBits(m_position.BitsAll()) > TB_LARGEST)
        return 0;

    auto result =
        tb_probe_root(m_position.BitsAll(WHITE), m_position.BitsAll(BLACK),
            m_position.Bits(KW) | m_position.Bits(KB),
            m_position.Bits(QW) | m_position.Bits(QB),
            m_position.Bits(RW) | m_position.Bits(RB),
            m_position.Bits(BW) | m_position.Bits(BB),
            m_position.Bits(NW) | m_position.Bits(NB),
            m_position.Bits(PW) | m_position.Bits(PB),
            m_position.Fifty(),
            m_position.CanCastle(m_position.Side(), KINGSIDE) || m_position.CanCastle(m_position.Side(), QUEENSIDE),
            0,
            m_position.Side() == WHITE, nullptr);

    //
    //  Validate result of a probe, if uncertain, return 0 and fallback to igel's main search at root
    //

    if (result == TB_RESULT_FAILED || result == TB_RESULT_CHECKMATE || result == TB_RESULT_STALEMATE)
        return 0;

    //
    // Different board representation
    //

    auto to = abs(int(TB_GET_TO(result)) - 63);
    auto from = abs(int(TB_GET_FROM(result)) - 63);
    auto promoted = TB_GET_PROMOTES(result);
    auto ep = TB_GET_EP(result);

    Move tableBaseMove;

    assert(!ep);

    PIECE piece = m_position[from];
    PIECE capture = m_position[to];

    if (!promoted && !ep)
        tableBaseMove = Move(from, to, piece, capture);
    else {
        switch (promoted)
        {
        case TB_PROMOTES_QUEEN:
            tableBaseMove = Move(from, to, m_position[from], capture, QW | m_position.Side());
            break;
        case TB_PROMOTES_ROOK:
            tableBaseMove = Move(from, to, m_position[from], capture, RW | m_position.Side());
            break;
        case TB_PROMOTES_BISHOP:
            tableBaseMove = Move(from, to, m_position[from], capture, BW | m_position.Side());
            break;
        case TB_PROMOTES_KNIGHT:
            tableBaseMove = Move(from, to, m_position[from], capture, NW | m_position.Side());
            break;
        default:
            assert(false);
            break;
        }
    }

    //
    //  Sanity check, make sure move generated by tablebase probe is a valid one
    //

    MoveList moves;
    if (m_position.InCheck())
        GenMovesInCheck(m_position, moves);
    else
        GenAllMoves(m_position, moves);

    auto mvSize = moves.Size();
    for (size_t i = 0; i < mvSize; ++i) {
        if (moves[i].m_mv == tableBaseMove)
            return tableBaseMove;
    }

    assert(false);
    return 0;
}

void Search::startWorkerThreads(Time time)
{
    for (unsigned int i = 0; i < m_thc; ++i) {
        m_threadParams[i].m_readyMutex.lock();

        m_threadParams[i].m_nodes = 0;
        m_threadParams[i].m_selDepth = 0;
        m_threadParams[i].m_tbHits = 0;
        m_threadParams[i].setTime(time);
        m_threadParams[i].setLevel(m_level);
        m_threadParams[i].m_t0 = m_t0;
        m_threadParams[i].m_flags = m_flags;
        m_threadParams[i].m_smpThreadExit = false;

        m_threadParams[i].m_lazyDepth = 1;
        m_threadParams[i].m_index = i + 1;
        m_threadParams[i].m_readyMutex.unlock();

        m_threadParams[i].m_lazycv.notify_one();
    }
}

void Search::indicateWorkersStop()
{
    for (unsigned int i = 0; i < m_thc; ++i) {
        m_threadParams[i].m_smpThreadExit = true;
        m_threadParams[i].m_flags |= TERMINATED_BY_LIMIT;
    }
}

void Search::stopWorkerThreads()
{
    indicateWorkersStop();

    for (unsigned int i = 0; i < m_thc; ++i) {
        while (m_threadParams[i].m_lazyDepth)
            ;
    }
}

void Search::waitUntilCompletion()
{
    m_waitStarted = true;

    if (m_principalSearcher && (m_flags & MODE_ANALYZE)) {
        while ((m_flags & SEARCH_TERMINATED) == 0)
            ; // we must wait explicitely for stop command
    }

    m_waitStarted = false;
}

void Search::isReady()
{
    indicateWorkersStop();
    m_flags |= TERMINATED_BY_USER;
    std::unique_lock<std::mutex> lk(m_readyMutex);
    std::cout << "readyok" << std::endl;
}

void Search::stopPrincipalSearch()
{
    m_flags |= TERMINATED_BY_USER;
}

void Search::startPrincipalSearch(Time time, bool ponder)
{
    m_principalSearcher = true;

    m_readyMutex.lock();
    setTime(time);
    m_lazyDepth = 1;
    m_lazyPonder = ponder;
    m_readyMutex.unlock();
    m_lazycv.notify_one();

    if (!m_principalThread)
        m_principalThread.reset(new std::thread(&Search::lazySmpSearcher, this));
}

void Search::startSearch(Time time, int depth, bool ponderSearch)
{
    m_t0 = GetProcTime();
    m_time = time;
    m_ponderTime = time;
    m_iterPVSize = 0;
    m_nodes = 0;
    m_selDepth = 0;
    m_tbHits = 0;
    m_limitCheck = 1023;
    m_waitStarted = false;

    if (m_principalSearcher) {
        if (ponderSearch)
            m_time.setPonderMode(true);

        m_flags = (m_time.getTimeMode() == Time::TimeControl::Infinite ? MODE_ANALYZE : MODE_PLAY);
    }
    else
        m_time.setPonderMode(true); // always run smp threads in analyze mode

    memset(&m_pv, 0, sizeof(m_pv));
    m_time.resetAdjustment();

    Move ponder{};

    auto printBestMove = [](Search * pthis, Position & pos, Move m, Move p) {

        if (m)
            cout << "bestmove " << MoveToStrLong(m);

        if (!p)
            p = pthis->forceFetchPonder(pos, m);

        if (p)
            cout << " ponder " << MoveToStrLong(p);

        cout << endl;
    };

    if (m_principalSearcher) {

        //
        //  Check if game is over
        //

        string result, comment;
        int legalMoves = 0;
        if (isGameOver(m_position, result, comment, m_best, legalMoves)) {
            waitUntilCompletion();
            cout << result << " " << comment << endl << endl;
            printBestMove(this, m_position, m_best, ponder);
            return;
        }

        //
        //  Check if only a single legal move possible at this position
        //

        if (legalMoves == 1) {
            waitUntilCompletion();
            printBestMove(this, m_position, m_best, ponder);
            return;
        }

        //
        //  Probe tablebases/tt at root
        //

        auto bestTb = tableBaseRootSearch();

        if (bestTb) {
            waitUntilCompletion();
            printBestMove(this, m_position, bestTb, ponder);
            return;
        }
    }

    //
    //  Perform search for a best move
    //

    EVAL score = DRAW_SCORE;
    auto maxDepth = m_level == MAX_LEVEL ? MAX_PLY : ((MAX_PLY * m_level) / MAX_LEVEL);

    //
    //  Start worker threads if Threads option is configured
    //

    if (m_thc)
        startWorkerThreads(time);

    const int cycle = m_index % m_SMPCycles;
    uint64_t sumNodes = 0;
    uint64_t sumHits = 0;
    uint64_t nps = 0;

    for (m_depth = depth; m_depth < maxDepth; ++m_depth)
    {
        //
        // Occasionally skip depths by smp threads (laser & ethereal method)
        //

        if (!m_principalSearcher && (m_index + cycle) % m_skipDepths[cycle] == 0)
            m_depth += m_skipSize[cycle];

        //
        //  Make a search
        //

        EVAL aspiration = m_depth >= 4 ? 5 : CHECKMATE_SCORE;

        EVAL alpha = std::max(score - aspiration, -CHECKMATE_SCORE);
        EVAL beta  = std::min(score + aspiration, CHECKMATE_SCORE);

        while (aspiration <= CHECKMATE_SCORE) {
            score = abSearch(alpha, beta, m_depth, 0, false, false, true);

            if (m_flags & SEARCH_TERMINATED)
                break;

            memcpy(m_iterPV, m_pv[0], m_pvSize[0] * sizeof(Move));
            m_iterPVSize = m_pvSize[0];

            if (m_iterPVSize && m_pv[0][0]) {
                m_best = m_pv[0][0];

                if (m_iterPVSize > 1 && m_pv[0][1])
                    ponder = m_pv[0][1];
                else
                    ponder = 0;
            }

            aspiration += 2 + aspiration / 2;
            if (score <= alpha)
            {
                beta = (alpha + beta) / 2;
                alpha = std::max(score - aspiration, -CHECKMATE_SCORE);
            }
            else if (score >= beta)
                beta = std::min(score + aspiration, CHECKMATE_SCORE);
            else
                break;
        }

        if (m_flags & SEARCH_TERMINATED)
            break;

        //
        //  Update node statistic from all workers
        //

        if (m_principalSearcher) {
            sumNodes = m_nodes;
            sumHits = m_tbHits;
            for (unsigned int i = 0; i < m_thc; ++i) {
                sumNodes += m_threadParams[i].m_nodes;
                sumHits += m_threadParams[i].m_tbHits;
            }
        }

        if (m_principalSearcher)
            printPV(m_position, m_depth, m_selDepth, score, m_pv[0], m_pvSize[0], m_best, sumNodes, sumHits, nps);

        //
        //  Check time limits
        //

        auto dt = GetProcTime() - m_t0;

        if (dt > 1000)
            nps = 1000 * sumNodes / dt;

        if (m_time.getTimeMode() == Time::TimeControl::TimeLimit && dt >= m_time.getSoftLimit()) {
            m_flags |= TERMINATED_BY_LIMIT;

            if (g_log) {
                stringstream ss;
                ss << "Search stopped by stSoft, dt = " << dt;
                Log(ss.str());
            }

            break;
        }

        if (m_time.getTimeMode() == Time::TimeControl::DepthLimit && m_depth >= static_cast<int>(m_time.getDepthLimit())) {
            m_flags |= TERMINATED_BY_LIMIT;
            break;
        }
    }

    //
    //  Stop worker threads if necessary
    //

    if (m_thc)
        stopWorkerThreads();

    //
    //  In case we are pondering, wait until stop/ponderhit is issued
    //

    waitUntilCompletion();

    if (m_principalSearcher)
        printBestMove(this, m_position, m_best, ponder);
}

void Search::setPonderHit()
{
    m_t0    = GetProcTime();
    m_time  = m_ponderTime;
    m_flags = (m_time.getTimeMode() == Time::TimeControl::Infinite ? MODE_ANALYZE : MODE_PLAY);

    if (m_waitStarted)
        stopPrincipalSearch();
}

void Search::setSyzygyDepth(int depth)
{
    m_syzygyDepth = depth;
}

void Search::setLevel(int level)
{
    m_level = level;
}

void Search::setThreadCount(unsigned int threads)
{
    if (threads == m_thc)
        return;

    releaseHelperThreads();

    m_thc = threads;
    m_threads.reset(new std::thread[threads]);
    m_threadParams.reset(new Search[threads]);

    for (unsigned int i = 0; i < m_thc; ++i)
        m_threads[i] = std::thread(&Search::lazySmpSearcher, &m_threadParams[i]);
}

unsigned int Search::getThreadsCount()
{
    return m_thc + 1;
}

void Search::lazySmpSearcher()
{
    while (!m_terminateSmp)
    {
        bool ponder;

        {
            std::unique_lock<std::mutex> lk(m_readyMutex);
            m_lazycv.wait(lk, std::bind(&Search::getIsLazySmpWork, this));

            if (m_terminateSmp)
                return;

            m_depth = m_lazyDepth;
            ponder = m_lazyPonder;

            startSearch(m_time, m_depth, ponder);
            resetLazySmpWork();
        }
    }
}

void Search::releaseHelperThreads()
{
    for (unsigned int i = 0; i < m_thc; ++i)
    {
        if (m_threads[i].joinable()) {
            m_threadParams[i].m_terminateSmp = true;
            {
                std::unique_lock<std::mutex> lk(m_threadParams[i].m_readyMutex);
                m_threadParams[i].m_lazyDepth = 1;
            }
            m_threadParams[i].m_lazycv.notify_one();
            m_threads[i].join();
        }
    }
}

bool Search::setFEN(const std::string& fen)
{
    for (unsigned int i = 0; i < m_thc; ++i)
        m_threadParams[i].m_position.SetFEN(fen);

    return m_position.SetFEN(fen);
}

bool Search::setInitialPosition()
{
    for (unsigned int i = 0; i < m_thc; ++i)
        m_threadParams[i].m_position.SetInitial();

    m_position.SetInitial();

    return true;
}

bool Search::makeMove(Move mv)
{
    for (unsigned int i = 0; i < m_thc; ++i)
        m_threadParams[i].m_position.MakeMove(mv);

    return m_position.MakeMove(mv);
}