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

#include "nnue.h"
#include "moves.h"
#include "notation.h"
#include "search.h"
#include "utils.h"
#if defined (SYZYGY_SUPPORT)
#include "fathom/tbprobe.h"
#endif
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

Search::Search() :
    m_nodes(0),
    m_tbHits(0),
    m_limitCheck(1023),
    m_t0(0),
    m_flags(0),
    m_depth(0),
    m_syzygyDepth(0),
    m_selDepth(0),
    m_principalSearcher(false),
    m_thc(0),
    m_threads(nullptr),
    m_threadParams(nullptr),
    m_lazyDepth(0),
    m_smpThreadExit(false),
    m_lazyPonder(false),
    m_terminateSmp(false),
    m_level(DEFAULT_LEVEL),
    m_ponderHit(false),
    m_score(-CHECKMATE_SCORE),
    m_best(0),
    m_ponder(0)
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
        if (m_flags & MODE_PLAY) {
            if (m_time.getTimeMode() == Time::TimeControl::TimeLimit && dt >= m_time.getHardLimit())
                m_flags |= TERMINATED_BY_LIMIT;
        }
    }

    return (m_flags & SEARCH_TERMINATED);
}

bool Search::isDraw()
{
    if ((m_position.Repetitions() >= 2) || (m_position.Fifty() >= 100))
        return true;

    if (!m_position.Count(PW) && !m_position.Count(PB)) {
        if (m_position.MatIndex(WHITE) < 5 && m_position.MatIndex(BLACK) < 5)
            return true;
    }

    return false;
}

EVAL Search::abSearch(EVAL alpha, EVAL beta, int depth, int ply, bool isNull, bool rootNode, bool cutNode, Move skipMove/*= 0*/)
{
    //
    //   qsearch
    //

    if (depth <= 0 && m_level > MEDIUM_LEVEL)
        return qSearch(alpha, beta, ply, 0, isNull);

    TTable::instance().prefetchEntry(m_position.Hash());

    ++m_nodes;
    m_pvSize[ply]  = 0;
    m_selDepth     = std::max(ply, m_selDepth);

    if (!rootNode) {

        if (checkLimits())
            return DRAW_SCORE;

        if ((ply > MAX_PLY - 2) || isDraw())
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
    TEntry hEntry{};

    EVAL ttScore = 0;
    auto onPV    = (beta - alpha > 1);
    auto ttHit   = !skipMove && !isNull && ProbeHash(hEntry);

    if (ttHit) {
        ttScore = hEntry.m_data.score;
        if (ttScore > CHECKMATE_SCORE - 50 && ttScore <= CHECKMATE_SCORE)
            ttScore -= ply;
        if (ttScore < -CHECKMATE_SCORE + 50 && ttScore >= -CHECKMATE_SCORE)
            ttScore += ply;
        if (hEntry.m_data.depth >= depth && (depth == 0 || !onPV)) {
            if (!onPV && (m_position.Fifty() < 90) && (hEntry.m_data.type == HASH_EXACT
                || (hEntry.m_data.type == HASH_BETA && ttScore >= beta)
                || (hEntry.m_data.type == HASH_ALPHA && ttScore <= alpha)))
                return ttScore;
        }

        hashMove = hEntry.m_data.move;
    }

    //
    //  tablebase probe
    //

#if defined (SYZYGY_SUPPORT)
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
#endif

    auto inCheck     = m_position.InCheck();
    EVAL staticEval  = inCheck ? -CHECKMATE_SCORE + ply : (isNull ? -m_evalStack[ply - 1] + 2 * Evaluator::Tempo : m_evaluator->evaluate(m_position));
    EVAL bestScore   = staticEval;

    m_evalStack[ply] = staticEval;

    if (ttHit && !inCheck) {
        if ((hEntry.m_data.type == HASH_BETA && ttScore > staticEval) ||
            (hEntry.m_data.type == HASH_ALPHA && ttScore < staticEval) ||
            (hEntry.m_data.type == HASH_EXACT))
            bestScore = ttScore;
    }

    auto improving = ply >= 2 && staticEval > m_evalStack[ply - 2];

    //
    //  apply prunning when we are not in check and not on PV
    //

    if (!inCheck && !onPV) {

        //
        //   razoring
        //

        if (depth <= 2 && staticEval + 150 < alpha)
            return qSearch(alpha, beta, ply, 0);

        //
        //  static null move pruning
        //

        if (depth <= 8 && bestScore - 85 * (depth - improving) >= beta)
            return bestScore;

        //
        //   null move
        //

        if (!isNull && depth >= 3 && bestScore >= beta && (!ttHit || !(hEntry.m_data.type == HASH_BETA) || ttScore >= beta) && m_position.NonPawnMaterial()) {
            int R = 5 + depth / 6 + std::min(3, (bestScore - beta) / 100);

            m_position.MakeNullMove();
            EVAL nullScore = -abSearch(-beta, -beta + 1, depth - R, ply + 1, true, false, !cutNode);
            m_position.UnmakeNullMove();

            if (nullScore >= beta)
                return isCheckMateScore(nullScore) ? beta : nullScore;
        }

        //
        //  probcut
        //

        auto betaCut = beta + 100;

        if (depth >= 5 && !(ttHit && hEntry.m_data.depth >= (depth - 4) && hEntry.m_data.score < betaCut)) {
            MoveList captureMoves;

            GenCapturesAndPromotions(m_position, captureMoves);
            MoveEval::sortMoves(this, captureMoves, hashMove, ply);

            auto captureMovesSize = captureMoves.Size();
            for (size_t i = 0; i < captureMovesSize; ++i) {
                if (skipMove == captureMoves[i].m_mv)
                    continue;

                if (MoveEval::SEE(this, captureMoves[i].m_mv) < betaCut - staticEval)
                    continue;

                if (m_position.MakeMove(captureMoves[i].m_mv)) {

                    auto score = -qSearch(-betaCut, -betaCut + 1, ply, 0);

                    if (score >= betaCut)
                        score = -abSearch(-betaCut, -betaCut + 1, depth - 4, ply + 1, false, false, !cutNode);

                    m_position.UnmakeMove();

                    if (score >= betaCut)
                        return score;
                }
            }
        }
    }

    //
    //  IID
    //

    if (depth >= 7 && (onPV || cutNode) && (!hashMove || hEntry.m_data.depth + 4 < depth))
        --depth;

    auto legalMoves = 0;
    bestScore = -CHECKMATE_SCORE + ply;
    U8 type = HASH_ALPHA;
    Move bestMove = hashMove;

    MoveList& mvlist = m_lists[ply];
    if (inCheck)
        GenMovesInCheck(m_position, mvlist);
    else
        GenAllMoves(m_position, mvlist);

    MoveEval::sortMoves(this, mvlist, hashMove, ply);
    auto mvSize = mvlist.Size();

    MoveList quietMoves;
    m_killerMoves[ply + 1][0] = m_killerMoves[ply + 1][1] = 0;
    auto quietsTried = 0;
    auto skipQuiets = false;

    for (size_t i = 0; i < mvSize; ++i) {

        Move mv = MoveEval::getNextBest(mvlist, i);

        if (mv == skipMove)
            continue;

        auto quietMove = !MoveEval::isTacticalMove(mv);
        History::HistoryHeuristics history{};

        if (!rootNode && bestScore > MATED_IN_MAX) {

            if (quietMove) {
                if (skipQuiets)
                    continue;

                History::fetchHistory(this, mv, ply, history);

                if (depth <= m_cmpDepth[improving] && history.cmhistory < m_cmpHistoryLimit[improving])
                    continue;

                if (depth <= m_fmpDepth[improving] && history.fmhistory < m_fmpHistoryLimit[improving])
                    continue;

                auto futilityMargin = staticEval + 90 * depth;

                if (futilityMargin <= alpha
                    && depth <= 8
                    && history.history + history.cmhistory + history.fmhistory < m_fpHistoryLimit[improving])
                    skipQuiets = true;

                if (depth <= m_lmpDepth && quietsTried >= m_lmpPruningTable[improving][depth])
                    skipQuiets = true;
            }

            if (depth <= 8 && !inCheck) {

                int seeMargin[2];

                static const int SEEQuietMargin = -60;
                static const int SEENoisyMargin = -10;

                seeMargin[0] = SEENoisyMargin * depth * depth;
                seeMargin[1] = SEEQuietMargin * depth;

                if (MoveEval::SEE(this, mv) < seeMargin[quietMove])
                    continue;
            }
        }

        int newDepth  = depth - 1;
        int extension = 0;

        //
        //  singular extensions
        //

        if (depth >= 8 && !skipMove && hashMove == mv && !rootNode && !isCheckMateScore(hEntry.m_data.score) && hEntry.m_data.type == HASH_BETA && hEntry.m_data.depth >= depth - 3) {
            auto betaCut = hEntry.m_data.score - depth;
            auto score = abSearch(betaCut - 1, betaCut, depth / 2, ply + 1, false, false, cutNode, mv);

            if (score < betaCut) {
                extension = 1;
                if (!onPV && score < betaCut - 50)
                    extension = 2;
            }
            else if (betaCut >= beta)
                return betaCut;
            else if (ttHit && ttScore >= beta)
                extension = -2; // negative extension
        }

        if (quietMove) {
            ++quietsTried;
            quietMoves.Add(mv);
        }

        if (m_position.MakeMove(mv)) {
            ++legalMoves;

            m_moveStack[ply]  = mv;
            m_pieceStack[ply] = mv.Piece();

            //
            //   extensions
            //

            newDepth += extensionRequired(m_position.InCheck(), onPV, history.cmhistory, history.fmhistory) + extension;

            //
            //   lmr
            //

            int reduction = 0;

            if (depth >= 3 && quietMove && legalMoves > 1 + 2 * rootNode) {
                reduction = m_logLMRTable[std::min(depth, 63)][std::min(legalMoves, 63)];

                reduction += cutNode;

                if (onPV)
                    reduction -= 2;

                reduction -= mv == m_killerMoves[ply][0]
                    || mv == m_killerMoves[ply][1];

                reduction -= std::max(-2, std::min(2, (history.history + history.cmhistory + history.fmhistory) / 5000));

                if (reduction >= newDepth)
                    reduction = newDepth - 1;
                else if (reduction < 0)
                    reduction = 0;
            }

            EVAL e;

            if (reduction) {
                e = -abSearch(-alpha - 1, -alpha, newDepth - reduction, ply + 1, false, false, true);

                if (e > alpha)
                    e = -abSearch(-alpha - 1, -alpha, newDepth, ply + 1, false, false, !cutNode);
            }
            else if (!onPV || legalMoves > 1)
                e = -abSearch(-alpha - 1, -alpha, newDepth, ply + 1, false, false, !cutNode);

            if (onPV && (legalMoves == 1 || e > alpha))
                e = -abSearch(-beta, -alpha, newDepth, ply + 1, false, false, false);

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
                        if (quietMove) {
                            History::updateHistory(this, quietMoves, ply, depth * depth);
                            History::setKillerMove(this, mv, ply);
                        }
                        break;
                    }
                }
            }
        }
    }

    TTable::instance().prefetchEntry(m_position.Hash());

    if (legalMoves == 0) {
        if (inCheck || skipMove)
            bestScore = -CHECKMATE_SCORE + ply;
        else
            bestScore = DRAW_SCORE;
    }

    assert((m_position.Fifty() >= 100) == false); // we must cut off at the begining of a node search for draws

    if (!skipMove)
        TTable::instance().record(bestMove, bestScore, depth, ply, type, m_position.Hash());

    return bestScore;
}

EVAL Search::qSearch(EVAL alpha, EVAL beta, int ply, int depth, bool isNull/* = false*/)
{
    ++m_nodes;
    m_pvSize[ply]   = 0;
    m_selDepth      = std::max(ply, m_selDepth);

    if (checkLimits())
        return DRAW_SCORE;

    TTable::instance().prefetchEntry(m_position.Hash());

    if ((ply > MAX_PLY - 2) || isDraw())
        return ((ply > MAX_PLY - 2) && !m_position.InCheck()) ? m_evaluator->evaluate(m_position) : DRAW_SCORE;

    Move hashMove{};
    TEntry hEntry{};
    EVAL ttScore;

    auto inCheck = m_position.InCheck();
    auto tteDepth = inCheck || depth >= 0 ? 0 : -1;
    auto ttHit = ProbeHash(hEntry);

    if (ttHit) {
        ttScore = hEntry.m_data.score;
        if (ttScore > CHECKMATE_SCORE - 50 && ttScore <= CHECKMATE_SCORE)
            ttScore -= ply;
        if (ttScore < -CHECKMATE_SCORE + 50 && ttScore >= -CHECKMATE_SCORE)
            ttScore += ply;
        if (hEntry.m_data.depth >= tteDepth) {
            auto onPV = (beta - alpha > 1);

            if (!onPV && (hEntry.m_data.type == HASH_EXACT
                || (hEntry.m_data.type == HASH_BETA && ttScore >= beta)
                || (hEntry.m_data.type == HASH_ALPHA && ttScore <= alpha)))
                return ttScore;
        }

        hashMove = hEntry.m_data.move;
    }

    EVAL bestScore;

    if (inCheck)
    {
        bestScore = -CHECKMATE_SCORE + ply;
    }
    else
    {
        bestScore = (isNull ? -m_evalStack[ply - 1] + 2 * Evaluator::Tempo : m_evaluator->evaluate(m_position));

        if (ttHit) {
            if ((hEntry.m_data.type == HASH_BETA && ttScore > bestScore)  ||
                (hEntry.m_data.type == HASH_ALPHA && ttScore < bestScore) ||
                (hEntry.m_data.type == HASH_EXACT))
                bestScore = ttScore;
        }

        if (bestScore >= beta) {
            if (!ttHit)
                TTable::instance().record(0, bestScore, -5, ply, HASH_BETA, m_position.Hash());
            return bestScore;
        }

        if (alpha < bestScore)
            alpha = bestScore;
    }

    MoveList& mvlist = m_lists[ply];
    if (inCheck)
        GenMovesInCheck(m_position, mvlist);
    else
        GenCapturesAndPromotions(m_position, mvlist);

    MoveEval::sortMoves(this, mvlist, hashMove, ply);
    auto mvSize = mvlist.Size();
    Move bestMove = hashMove;
    U8 type = HASH_ALPHA;

    for (size_t i = 0; i < mvSize; ++i) {
        Move mv = MoveEval::getNextBest(mvlist, i);

        if (!inCheck && MoveEval::SEE(this, mv) < 0)
            continue;

        if (m_position.MakeMove(mv)) {

            auto e = -qSearch(-beta, -alpha, ply + 1, depth - 1);
            m_position.UnmakeMove();

            if (m_flags & SEARCH_TERMINATED)
                return DRAW_SCORE;

            if (e > bestScore) {
                bestScore = e;
                if (e > alpha) {
                    alpha = e;
                    bestMove = mv;
                    type = HASH_EXACT;
                }
            }

            if (alpha >= beta) {
                type = HASH_BETA;
                break;
            }
        }
    }

    TTable::instance().record(bestMove, bestScore, tteDepth, ply, type, m_position.Hash());
    return bestScore;
}

void Search::setInitial()
{
    m_position.SetInitial();

    for (unsigned int i = 0; i < m_thc; ++i)
        m_threadParams[i].m_position.SetInitial();
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
    for (unsigned int j = 0; j < sizeof(m_moveStack) / sizeof(Move); ++j)
        m_moveStack[j].reset();

    memset(m_pieceStack, 0, sizeof(m_pieceStack));
    memset(m_followTable, 0, sizeof(m_followTable));
    memset(m_evalStack, 0, sizeof(m_evalStack));
    memset(m_pvSize, 0, sizeof(m_pvSize));

    for (unsigned int i = 0; i < m_thc; ++i) {

        for (unsigned int j = 0; j < sizeof(m_threadParams[i].m_moveStack) / sizeof(Move); ++j)
            m_threadParams[i].m_moveStack[j].reset();

        memset(m_threadParams[i].m_pieceStack, 0, sizeof(m_pieceStack));
        memset(m_threadParams[i].m_followTable, 0, sizeof(m_followTable));
        memset(m_threadParams[i].m_evalStack, 0, sizeof(m_evalStack));
        memset(m_threadParams[i].m_pvSize, 0, sizeof(m_threadParams[i].m_pvSize));
    }
}

int Search::extensionRequired(bool inCheck, bool onPV, int cmhistory, int fmhistory)
{
    if (!onPV && cmhistory >= 10000 && fmhistory >= 10000)
        return 1;
    else if (inCheck)
        return 1;
    return 0;
}

bool Search::isGameOver(Position & pos, std::string & result, std::string & comment, Move & bestMove, int & legalMoves)
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

    std::cout << "info depth " << iter << " seldepth " << selDepth;

    if (abs(score) >= (CHECKMATE_SCORE - MAX_PLY))
        std::cout << " score mate" << ((score >= 0) ? " " : " -") << ((CHECKMATE_SCORE - abs(score)) / 2) + 1;
    else
        std::cout << " score cp " << score;

    std::cout << " time " << dt;
    std::cout << " nodes " << sumNodes;
    std::cout << " tbhits " << sumHits;

    if (nps)
        std::cout << " nps " << nps;

    std::cout << " pv";

    if (pvSize > 0) {
        for (int i = 0; i < pvSize; ++i)
            std::cout << " " << MoveToStrLong(pv[i]);
    }
    else
        std::cout << " " << MoveToStrLong(mv);

    std::cout << std::endl;
}

bool Search::ProbeHash(TEntry & hentry)
{
    auto hash  = m_position.Hash();
    return TTable::instance().retrieve(hash, hentry);
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
    if (!m_principalSearcher)
        return;

    while ((m_flags & MODE_ANALYZE) && (m_flags & SEARCH_TERMINATED) == 0)
        ; // we must wait explicitely for stop command or a ponderhit
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
    m_ponderHit = false;
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

uint64_t Search::startSearch(Time time, int depth, bool ponderSearch, bool bench)
{
    m_nodes = 0;
    m_selDepth = 0;
    m_tbHits = 0;
    m_limitCheck = 1023;

    if (!m_ponderHit) {
        m_t0 = GetProcTime();
        m_time = time;
        m_ponderTime = time;
        if (bench) {
            m_flags = MODE_SILENT | MODE_PLAY;
        }
        else {
            if (m_principalSearcher) {
                if (ponderSearch)
                    m_time.setPonderMode(true);

                m_flags = (m_time.getTimeMode() == Time::TimeControl::Infinite ? MODE_ANALYZE : MODE_PLAY);
            }
            else
                m_time.setPonderMode(true); // always run smp threads in analyze mode
        }
    }

    memset(m_pvPrev, 0, sizeof(m_pvPrev));
    memset(m_pvSizePrev, 0, sizeof(m_pvSizePrev));
    memset(&m_pv, 0, sizeof(m_pv));
    m_time.resetAdjustment();

    m_ponder = 0;
    m_best   = 0;

    auto printBestMove = [](Search * pthis, Position & pos, Move m, Move p) {

        if (m)
            std::cout << "bestmove " << MoveToStrLong(m);

        if (p)
            std::cout << " ponder " << MoveToStrLong(p);

        std::cout << std::endl;
    };

    //
    //  Perform search for a best move
    //

    m_score = DRAW_SCORE;
    auto maxDepth = m_level == MAX_LEVEL ? MAX_PLY : ((MAX_PLY * m_level) / MAX_LEVEL);

    //
    //  Start worker threads if Threads option is configured
    //

    if (m_thc)
        startWorkerThreads(time);

    uint64_t sumNodes = 0;
    uint64_t sumHits = 0;
    uint64_t nps = 0;

    for (m_depth = depth; m_depth < maxDepth; ++m_depth) {

        //
        //  Make a search
        //

        EVAL aspiration = m_depth >= 4 ? 5 : CHECKMATE_SCORE;

        EVAL alpha = std::max(m_score - aspiration, -CHECKMATE_SCORE);
        EVAL beta  = std::min(m_score + aspiration, CHECKMATE_SCORE);

        while (aspiration <= CHECKMATE_SCORE) {
            auto score = abSearch(alpha, beta, m_depth, 0, false, true, false);

            if (m_flags & SEARCH_TERMINATED)
                break;

            m_score = score;

            if (m_pvSize[0] && m_pv[0][0]) {
                m_best = m_pv[0][0];

                if (m_pvSize[0] > 1 && m_pv[0][1]) {
                    m_ponder = m_pv[0][1];
                    memcpy(m_pvPrev, m_pv, sizeof(m_pv));
                    memcpy(m_pvSizePrev, m_pvSize, sizeof(m_pvSize));
                }
                else
                    m_ponder = 0;
            }

            aspiration += 2 + aspiration / 2;
            if (m_score <= alpha)
            {
                beta = (alpha + beta) / 2;
                alpha = std::max(m_score - aspiration, -CHECKMATE_SCORE);
            }
            else if (m_score >= beta)
                beta = std::min(m_score + aspiration, CHECKMATE_SCORE);
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
            m_time.adjust(m_score, m_depth);
        }

        if (!(m_flags & MODE_SILENT) && m_principalSearcher)
            printPV(m_position, m_depth, m_selDepth, m_score, m_pv[0], m_pvSize[0], m_best, sumNodes, sumHits, nps);

        //
        //  Check time limits
        //

        auto dt = GetProcTime() - m_t0;

        if (dt > 1000)
            nps = 1000 * sumNodes / dt;

        if (m_time.getTimeMode() == Time::TimeControl::TimeLimit && dt >= m_time.getSoftLimit()) {
            m_flags |= TERMINATED_BY_LIMIT;
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

    if (m_thc) {
        stopWorkerThreads();

        //
        //  Pickup the best thread based on eval/depth
        //

        auto worst = m_score;

        //
        //  Calculate worst score yet
        //

        for (unsigned int i = 0; i < m_thc; ++i)
            worst = std::min(worst, m_threadParams[i].m_score);

        int64_t bestSoFar = (static_cast<int64_t>(m_score) - static_cast<int64_t>(worst) + 20) * static_cast<int64_t>(m_depth);

        typedef struct tagStat {
            tagStat(Move p, EVAL s, int d, int sd, const Move* pv_, int pvSize_) {
                ponder = p;
                score = s;
                depth = d;
                selDepth = sd;
                pvSize = pvSize_;
                memcpy(pv, pv_, pvSize * sizeof(Move));
            }
            tagStat() {
                ponder = 0;
                score = 0;
                depth = 0;
                selDepth = 0;
                pvSize = 0;
            }
            Move ponder;
            EVAL score;
            int depth;
            int selDepth;
            Move pv[MAX_PLY];
            int  pvSize;
        }Stat;

        std::map<Move, std::pair<int64_t, Stat>> votes;
        votes[m_best] = std::make_pair(bestSoFar, Stat{ m_ponder, m_score, m_depth, m_selDepth, m_pvPrev[0], m_pvSizePrev[0] });

        //
        //  Calculate the voting map
        //

        for (unsigned int i = 0; i < m_thc; ++i) {
            votes[m_threadParams[i].m_best].first  += (static_cast<int64_t>(m_threadParams[i].m_score) - static_cast<int64_t>(worst) + 20) * static_cast<int64_t>(m_threadParams[i].m_depth);
            votes[m_threadParams[i].m_best].second = Stat{ m_threadParams[i].m_ponder , m_threadParams[i].m_score, m_threadParams[i].m_depth, m_threadParams[i].m_selDepth, m_threadParams[i].m_pvPrev[0], m_threadParams[i].m_pvSizePrev[0] };
        }

        //
        //  Democracy in action: pick-up the most voted best based on highest score
        //

        for (const auto & vote : votes) {
            if (vote.first && (vote.second.first > bestSoFar)) { // select move with highest score
                bestSoFar = vote.second.first;                   // memorize highest score so far
                m_best = vote.first;                             // store the best move
                m_ponder = vote.second.second.ponder;            // store the best ponder
                m_score = vote.second.second.score;              // store statistics
                m_depth = std::max(m_depth, vote.second.second.depth);
                m_selDepth = std::max(m_selDepth, vote.second.second.selDepth);
                m_pvSizePrev[0] = vote.second.second.pvSize;
                memcpy(m_pvPrev[0], vote.second.second.pv, m_pvSizePrev[0] * sizeof(Move));
            }
        }

        printPV(m_position, m_depth, m_selDepth, m_score, m_pvPrev[0], m_pvSizePrev[0], m_best, sumNodes, sumHits, nps);
    }

    //
    //  In case we are pondering, wait until stop/ponderhit is issued
    //

    waitUntilCompletion();
    m_ponderHit = false;

    if (!(m_flags & MODE_SILENT) && m_principalSearcher) {
        if (!m_thc)
            printPV(m_position, m_depth, m_selDepth, m_score, m_pv[0], m_pvSize[0], m_best, sumNodes, sumHits, nps);
        printBestMove(this, m_position, m_best, m_ponder);
    }

    return m_nodes;
}

void Search::setPonderHit()
{
    m_ponderHit = true;
    m_flags     = MODE_PLAY;
    m_t0        = GetProcTime();
    m_time      = m_ponderTime;
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