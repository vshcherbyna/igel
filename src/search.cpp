/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2002-2018 Vladimir Medvedev <vrm@bk.ru> (GreKo author)
*  Copyright (C) 2018-2019 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

const U8 HASH_ALPHA = 0;
const U8 HASH_EXACT = 1;
const U8 HASH_BETA = 2;

Search::Search() :
    m_nodes(0),
    m_tbHits(0),
    m_t0(0),
    m_flags(0),
    m_depth(0),
    m_syzygyDepth(0),
    m_selDepth(0),
    m_iterPVSize(0),
    m_principalSearcher(false),
    m_thc(0),
    m_threads(nullptr),
    m_threadParams(nullptr),
    m_lazyDepth(0),
    m_lazyAlpha(-INFINITY_SCORE),
    m_lazyBeta(INFINITY_SCORE),
    m_bestSmpEval(0),
    m_smpThreadExit(false),
    m_terminateSmp(false)
{
    for (int depth = 1; depth < 64; depth++)
        for (int moves = 1; moves < 64; moves++)
            m_logLMRTable[depth][moves] = 0.75 + log(depth) * log(moves) / 2.25;
}

Search::~Search()
{
    releaseHelperThreads();
}

bool Search::CheckLimits(bool onPv, int depth, EVAL score)
{
    if (m_flags & SEARCH_TERMINATED)
        return true;

    if (m_smpThreadExit) {
        m_flags |= TERMINATED_BY_LIMIT;
        return true;
    }

    if (m_time.getTimeMode() == Time::TimeControl::NodesLimit)
    {
        if (m_nodes >= m_time.getNodesLimit())
            m_flags |= TERMINATED_BY_LIMIT;

        return (m_flags & SEARCH_TERMINATED);
    }

    U32 dt = GetProcTime() - m_t0;

    if (m_flags & MODE_PLAY)
    {
        //m_time.adjust(onPv, depth, score);
        if (m_time.getTimeMode() == Time::TimeControl::TimeLimit && dt >= m_time.getHardLimit())
        {
            m_flags |= TERMINATED_BY_LIMIT;

            stringstream ss;
            ss << "Search stopped by stHard, dt = " << dt;
            Log(ss.str());
        }
    }

    return (m_flags & SEARCH_TERMINATED);
}

void Search::ProcessInput(const string& s)
{
    vector<string> tokens;
    Split(s, tokens);
    if (tokens.empty())
        return;

    string cmd = tokens[0];

    if (m_flags & MODE_PLAY)
    {
        if (cmd == "?")
            m_flags |= TERMINATED_BY_USER;
        else if (Is(cmd, "result", 1))
        {
            //m_iterPVSize = 0;
            m_flags |= TERMINATED_BY_USER;
        }
    }

    if (Is(cmd, "exit", 1))
        m_flags |= TERMINATED_BY_USER;
    else if (Is(cmd, "quit", 1))
        exit(0);
    else if (Is(cmd, "stop", 1))
    {
        for (unsigned int i = 0; i < m_thc; ++i)
            m_threadParams[i].m_flags |= TERMINATED_BY_USER;
        m_flags |= TERMINATED_BY_USER;
    }
}

void Search::CheckInput()
{
    if (InputAvailable())
    {
        string s;
        getline(cin, s);
        ProcessInput(s);
    }
}

void Search::setPosition(Position & pos)
{
    m_position = pos;
}

EVAL Search::searchRoot(EVAL alpha, EVAL beta, int depth)
{
    int ply = 0;
    m_pvSize[ply] = 0;
    assert(depth > 0);
    Move hashMove = 0;

    int legalMoves = 0;
    Move bestMove = 0;
    U8 type = HASH_ALPHA;
    bool inCheck = m_position.InCheck();
    bool onPV = (beta - alpha > 1);

    if (m_pv[0][0])
        hashMove = m_pv[0][0];

    MoveList& mvlist = m_lists[ply];
    if (inCheck)
        GenMovesInCheck(m_position, mvlist);
    else
        GenAllMoves(m_position, mvlist);

    MoveEval::sortMoves(this, mvlist, hashMove, ply);
    auto mvSize = mvlist.Size();

    //
    //  Different move ordering for lazy smp threads
    //

    if (!m_principalSearcher && depth == 1 && mvSize >= 2) {
        int j = 0;
        for (size_t i = mvSize - 1; i > 0; --i) {
            mvlist.Swap(i, j++);
        }
    }

    std::vector<Move> quietMoves;

    for (size_t i = 0; i < mvSize; ++i)
    {
        if (m_principalSearcher)
            CheckInput();

        if (CheckLimits(onPV, depth, alpha))
            break;

        Move mv = MoveEval::getNextBest(mvlist, i);
        if (m_position.MakeMove(mv))
        {
            History::HistoryHeuristics history{};

            if (!MoveEval::isTacticalMove(mv)) {
                quietMoves.emplace_back(mv);
                History::fetchHistory(this, mv, ply, history);
            }

            ++m_nodes;
            ++legalMoves;
            m_moveStack[ply] = mv;
            m_pieceStack[ply] = mv.Piece();

            if ((m_principalSearcher) && ((GetProcTime() - m_t0) > 2000))
                cout << "info depth " << depth << " currmove " << MoveToStrLong(mv) << " currmovenumber " << legalMoves << endl;

            int newDepth = depth - 1;

            //
            //   EXTENSIONS
            //

            newDepth += extensionRequired(mv, m_position.LastMove(), inCheck, ply, onPV, quietMoves.size(), history.cmhistory, history.fmhistory);

            EVAL e;
            if (legalMoves == 1)
                e = -abSearch(-beta, -alpha, newDepth, ply + 1, false);
            else
            {
                e = -abSearch(-alpha - 1, -alpha, newDepth, ply + 1, false);
                if (e > alpha && e < beta)
                    e = -abSearch(-beta, -alpha, newDepth, ply + 1, false);
            }
            m_position.UnmakeMove();

            if (e > alpha)
            {
                alpha = e;
                bestMove = mv;
                type = HASH_EXACT;

                m_pv[ply][0] = mv;
                memcpy(m_pv[ply] + 1, m_pv[ply + 1], m_pvSize[ply + 1] * sizeof(Move));
                m_pvSize[ply] = 1 + m_pvSize[ply + 1];
                History::updateHistory(this, quietMoves, ply, depth * depth);
            }

            if (alpha >= beta)
            {
                type = HASH_BETA;
                if (!mv.Captured() && !mv.Promotion())
                    History::setKillerMove(this, mv, ply);
                break;
            }
        }
    }

    if (legalMoves == 0)
    {
        if (inCheck)
            alpha = -CHECKMATE_SCORE + ply;
        else
            alpha = DRAW_SCORE;
    }

    TTable::instance().record(bestMove, alpha, depth, ply, type, m_position.Hash());
    return alpha;
}

EVAL Search::abSearch(EVAL alpha, EVAL beta, int depth, int ply, bool isNull)
{
    if (ply > MAX_PLY - 2)
        return Evaluator::evaluate(m_position);

    m_pvSize[ply] = 0;

    if (!isNull && m_position.Repetitions() >= 2)
        return DRAW_SCORE;

    m_selDepth = std::max(ply, m_selDepth);

    //
    //   probe hash
    //

    Move hashMove{};
    TEntry hEntry;
    auto onPV = (beta - alpha > 1);

    if (ProbeHash(hEntry)) {
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

    if (TB_LARGEST && depth >= 2 && !m_position.Fifty() && !(m_position.CanCastle(m_position.Side(), KINGSIDE) || m_position.CanCastle(m_position.Side(), QUEENSIDE))) {
        auto pieces = CountBits(m_position.BitsAll());

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

    if (CheckLimits(onPV, depth, alpha))
        return -INFINITY_SCORE;

    bool inCheck = m_position.InCheck();

    //
    //   qsearch
    //

    if (!inCheck && depth <= 0)
        return qSearch(alpha, beta, ply);

    auto rootNode = ply == 0;

    if (!rootNode) {

        //
        // mate distance pruning
        //

        auto rAlpha = alpha > -CHECKMATE_SCORE + ply ? alpha : -CHECKMATE_SCORE + ply;
        auto rBeta = beta < CHECKMATE_SCORE - ply - 1 ? beta : CHECKMATE_SCORE - ply - 1;

        if (rAlpha >= rBeta)
            return rAlpha;
    }

    //
    //   futility
    //

    EVAL staticEval = m_evalStack[ply] = Evaluator::evaluate(m_position);

    bool nonPawnMaterial = m_position.NonPawnMaterial();
    static const EVAL MARGIN[4] = { 0, 50, 350, 550 };

    if (nonPawnMaterial && !onPV && !inCheck && depth >= 1 && depth <= 3) {
        if (staticEval <= alpha - MARGIN[depth])
            return qSearch(alpha, beta, ply);
        if (staticEval >= beta + MARGIN[depth])
            return beta;
    }

    //
    //   razoring
    //

    if (!onPV && !inCheck && depth <= 1 && staticEval + 325 < alpha)
        return qSearch(alpha, beta, ply);

    //
    //  static null move pruning
    //

    if (!onPV && !inCheck && depth <= 4 && (abs(beta - 1) > -INFINITY_SCORE + 100) && staticEval - 85 * depth > beta)
        return staticEval;

    //
    //   null move
    //

    if (!isNull && !onPV && !inCheck && depth >= 2 && nonPawnMaterial && staticEval >= beta) {
        int R = 4 + depth / 6 + min(3, (staticEval - beta) / 200);

        m_position.MakeNullMove();
        m_moveStack[ply] = 0;
        EVAL nullScore = -abSearch(-beta, -beta + 1, depth - R, ply + 1, true);
        m_position.UnmakeNullMove();

        if (nullScore >= beta)
            return beta;
    }

    //
    //  probcut
    //
    /*

    // interestingly it does not work out well in Igel

    if (!onPV && depth >= 5) {
        auto betaCut = beta + 100;
        MoveList captureMoves;
        GenCapturesAndPromotions(m_position, captureMoves);
        auto captureMovesSize = captureMoves.Size();

        for (size_t i = 0; i < captureMovesSize; ++i)
        {
            if (SEE(captureMoves[i].m_mv) < betaCut - staticEval)
                continue;

            if (m_position.MakeMove(captureMoves[i].m_mv)) {
                auto score = -abSearch(-betaCut, -betaCut + 1, depth - 4, ply + 1, isNull);
                m_position.UnmakeMove();

                if (score >= betaCut)
                    return score;
            }
        }
    }

    */

    //
    //   iid
    //

    if (onPV && hashMove == 0 && depth > 4)
    {
        abSearch(alpha, beta, depth - 4, ply, isNull);
        if (m_pvSize[ply] > 0)
            hashMove = m_pv[ply][0];
    }

    int legalMoves = 0;
    Move bestMove = 0;
    U8 type = HASH_ALPHA;

    MoveList& mvlist = m_lists[ply];
    if (inCheck)
        GenMovesInCheck(m_position, mvlist);
    else
        GenAllMoves(m_position, mvlist);

    MoveEval::sortMoves(this, mvlist, hashMove, ply);

    COLOR side = m_position.Side();
    auto lateEndgame = (m_position.MatIndex(side) < 5);
    auto mvSize = mvlist.Size();
    std::vector<Move> quietMoves;
    auto improving = ply >= 2 && staticEval > m_evalStack[ply - 2];

    for (size_t i = 0; i < mvSize; ++i) {
        Move mv = MoveEval::getNextBest(mvlist, i);
        if (m_position.MakeMove(mv))
        {
            ++m_nodes;
            ++legalMoves;

            auto quietMove = !MoveEval::isTacticalMove(mv);
            History::HistoryHeuristics history{};

            if (quietMove) {
                quietMoves.emplace_back(mv);
                History::fetchHistory(this, mv, ply, history);
            }

            m_moveStack[ply] = mv;
            m_pieceStack[ply] = mv.Piece();

            int newDepth = depth - 1;

            //
            //   extensions
            //

            newDepth += extensionRequired(mv, m_position.LastMove(), inCheck, ply, onPV, quietMoves.size(), history.cmhistory, history.fmhistory);
            auto extended = newDepth != (depth - 1);

            EVAL e;
            if (legalMoves == 1)
                e = -abSearch(-beta, -alpha, newDepth, ply + 1, false);
            else
            {
                //
                //   lmr
                //

                int reduction = 0;

                if (!improving && !lateEndgame && !onPV && quietMove && depth >= 3 && !extended && !m_position.InCheck() && !inCheck) {
                    reduction = m_logLMRTable[std::min(depth, 63)][std::min(legalMoves, 63)];

                    /*reduction += !improving;*/
                    reduction -= mv == m_killerMoves[ply][0]
                        || mv == m_killerMoves[ply][1];

                    reduction -= std::max(-2, std::min(2, (history.history + history.cmhistory + history.fmhistory) / 5000));

                    if (reduction < 0)
                        reduction = 0;
                }

                e = -abSearch(-alpha - 1, -alpha, newDepth - reduction, ply + 1, false);

                if (e > alpha && reduction > 0)
                    e = -abSearch(-alpha - 1, -alpha, newDepth, ply + 1, false);
                if (e > alpha && e < beta)
                    e = -abSearch(-beta, -alpha, newDepth, ply + 1, false);
            }
            m_position.UnmakeMove();

            if (m_flags & SEARCH_TERMINATED)
                break;

            if (e > alpha)
            {
                alpha = e;
                bestMove = mv;
                type = HASH_EXACT;

                m_pv[ply][0] = mv;
                memcpy(m_pv[ply] + 1, m_pv[ply + 1], m_pvSize[ply + 1] * sizeof(Move));
                m_pvSize[ply] = 1 + m_pvSize[ply + 1];
                History::updateHistory(this, quietMoves, ply, depth * depth);
            }

            if (alpha >= beta)
            {
                type = HASH_BETA;
                if (!mv.Captured())
                    History::setKillerMove(this, mv, ply);
                break;
            }
        }
    }

    if (legalMoves == 0) {
        if (inCheck)
            alpha = -CHECKMATE_SCORE + ply;
        else
            alpha = DRAW_SCORE;
    }
    else
    {
        if (m_position.Fifty() >= 100)
            alpha = DRAW_SCORE;
    }

    TTable::instance().record(bestMove, alpha, depth, ply, type, m_position.Hash());
    return alpha;
}

EVAL Search::qSearch(EVAL alpha, EVAL beta, int ply)
{
    if (ply > MAX_PLY - 2)
        return Evaluator::evaluate(m_position);

    m_pvSize[ply] = 0;
    m_selDepth = std::max(ply, m_selDepth);

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

        if (hEntry.type() == HASH_EXACT
            || (hEntry.type() == HASH_BETA && ttScore >= beta)
            || (hEntry.type() == HASH_ALPHA && ttScore <= alpha))
            return ttScore;

        hashMove = hEntry.move();
    }

    bool inCheck = m_position.InCheck();

    EVAL    bestScore,
        staticScore;

    if (inCheck)
    {
        bestScore = staticScore = -CHECKMATE_SCORE + ply;
    }
    else
    {
        bestScore = staticScore = ttHit ? hEntry.score() : Evaluator::evaluate(m_position);

        if (ttHit)
        {
            if ((hEntry.type() == HASH_BETA && ttScore > staticScore) ||
                (hEntry.type() == HASH_ALPHA && ttScore < staticScore) ||
                (hEntry.type() == HASH_EXACT))
                bestScore = ttScore;
        }

        if (bestScore >= beta)
            return bestScore;

        if (alpha < bestScore)
            alpha = bestScore;
    }

    if (CheckLimits((beta - alpha > 1), ply, alpha))
        return -INFINITY_SCORE;

    MoveList& mvlist = m_lists[ply];
    if (inCheck)
        GenMovesInCheck(m_position, mvlist);
    else
        GenCapturesAndPromotions(m_position, mvlist);

    MoveEval::sortMoves(this, mvlist, hashMove, ply);

    int legalMoves = 0;
    auto mvSize = mvlist.Size();
    Move bestMove = ttHit ? hEntry.move() : Move{};

    for (size_t i = 0; i < mvSize; ++i)
    {
        Move mv = MoveEval::getNextBest(mvlist, i);

        if (!inCheck && MoveEval::SEE(this, mv) < 0)
            continue;

        if (m_position.MakeMove(mv))
        {
            ++m_nodes;
            ++legalMoves;

            m_moveStack[ply] = mv;
            m_pieceStack[ply] = mv.Piece();

            EVAL e = -qSearch(-beta, -alpha, ply + 1);
            m_position.UnmakeMove();

            if (m_flags & SEARCH_TERMINATED)
                break;

            if (e > bestScore)
            {
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
    memset(m_pieceStack, 0, sizeof(m_moveStack));
    memset(m_followTable, 0, sizeof(m_followTable));
    memset(m_counterTable, 0, sizeof(m_counterTable));
    memset(m_evalStack, 0, sizeof(m_evalStack));

    for (unsigned int i = 0; i < m_thc; ++i) {
        memset(m_threadParams[i].m_moveStack, 0, sizeof(m_moveStack));
        memset(m_threadParams[i].m_pieceStack, 0, sizeof(m_pieceStack));
        memset(m_threadParams[i].m_followTable, 0, sizeof(m_followTable));
        memset(m_threadParams[i].m_counterTable, 0, sizeof(m_counterTable));
        memset(m_threadParams[i].m_evalStack, 0, sizeof(m_evalStack));
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
        if (mv.Piece() == PW && Row(mv.To()) == 1)
            return 1;
        else if (mv.Piece() == PB && Row(mv.To()) == 6)
            return 1;
        else if (onPV && lastMove && mv.To() == lastMove.To() && lastMove.Captured())
            return 1;
    }
    return 0;
}

bool Search::HaveSingleMove(Position& pos, Move & bestMove)
{
    MoveList mvlist;
    GenAllMoves(pos, mvlist);

    int legalMoves = 0;
    EVAL bestScore = -INFINITY_SCORE;
    auto mvSize = mvlist.Size();

    for (size_t i = 0; i < mvSize; ++i)
    {
        Move mv = mvlist[i].m_mv;
        if (pos.MakeMove(mv))
        {
            EVAL score = Evaluator::evaluate(pos);
            pos.UnmakeMove();

            if (score >= bestScore)
            {
                bestMove = mv;
                bestScore = score;
            }
            ++legalMoves;
            if (legalMoves > 1)
                break;
        }
    }

    return (legalMoves == 1);
}

bool Search::IsGameOver(Position& pos, string& result, string& comment)
{
    if (pos.Count(PW) == 0 && pos.Count(PB) == 0)
    {
        if (pos.MatIndex(WHITE) < 5 && pos.MatIndex(BLACK) < 5)
        {
            result = "1/2-1/2";
            comment = "{Insufficient material}";
            return true;
        }
    }

    MoveList mvlist;
    GenAllMoves(pos, mvlist);
    int legalMoves = 0;
    auto mvSize = mvlist.Size();

    for (size_t i = 0; i < mvSize; ++i)
    {
        Move mv = mvlist[i].m_mv;
        if (pos.MakeMove(mv))
        {
            ++legalMoves;
            pos.UnmakeMove();
            break;
        }
    }

    if (legalMoves == 0)
    {
        if (pos.InCheck())
        {
            if (pos.Side() == WHITE)
            {
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

    if (pos.Fifty() >= 100)
    {
        result = "1/2-1/2";
        comment = "{Fifty moves rule}";
        return true;
    }

    if (pos.Repetitions() >= 3)
    {
        result = "1/2-1/2";
        comment = "{Threefold repetition}";
        return true;
    }

    return false;
}

void Search::PrintPV(const Position& pos, int iter, int selDepth, EVAL score, const Move* pv, int pvSize, const string& sign)
{
    if (!pvSize)
        return;

    U32 dt = GetProcTime() - m_t0;

    cout << "info depth " << iter << " seldepth " << selDepth;
    if (abs(score) >= (CHECKMATE_SCORE - MAX_PLY))
        cout << " score mate" << ((score >= 0) ? " " : " -") << ((CHECKMATE_SCORE - abs(score)) / 2) + 1;
    else
        cout << " score cp " << score;
    cout << " time " << dt;
    cout << " nodes " << m_nodes;
    cout << " tbhits " << m_tbHits;

    if (pvSize > 0)
    {
        cout << " pv";
        for (int i = 0; i < pvSize; ++i)
            cout << " " << MoveToStrLong(pv[i]);
    }
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



Move Search::FirstLegalMove(Position& pos)
{
    MoveList mvlist;
    GenAllMoves(pos, mvlist);

    auto mvSize = mvlist.Size();
    for (size_t i = 0; i < mvSize; ++i)
    {
        Move mv = mvlist[i].m_mv;
        if (m_position.MakeMove(mv))
        {
            m_position.UnmakeMove();
            return mv;
        }
    }

    return 0;
}

Move Search::hashTableRootSearch()
{
    if (m_depth) {
        TEntry hEntry;

        if (ProbeHash(hEntry)) {
            if (hEntry.depth() >= m_depth) {
                if (hEntry.type() == HASH_EXACT) {
                    bool moved = false;
                    if (m_position.MakeMove(hEntry.move()))
                    {
                        moved = true;
                        m_position.UnmakeMove();
                    }

                    if (moved)
                        return hEntry.move();
                }
            }
        }
    }

    return {};
}

Move Search::tableBaseRootSearch()
{
    if (!TB_LARGEST || CountBits(m_position.BitsAll()) > TB_LARGEST)
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
    for (unsigned int i = 0; i < m_thc; ++i)
    {
        m_threadParams[i].m_lazyMutex.lock();

        m_threadParams[i].m_nodes = 0;
        m_threadParams[i].m_selDepth = 0;
        m_threadParams[i].m_tbHits = 0;
        m_threadParams[i].setPosition(m_position);
        m_threadParams[i].setTime(time);
        m_threadParams[i].m_t0 = m_t0;
        m_threadParams[i].m_flags = m_flags;
        m_threadParams[i].m_bestSmpEval = -INFINITY_SCORE;
        m_threadParams[i].m_smpThreadExit = false;

        m_threadParams[i].m_lazyAlpha = -INFINITY_SCORE;
        m_threadParams[i].m_lazyBeta = INFINITY_SCORE;
        m_threadParams[i].m_lazyDepth = 1;
        m_threadParams[i].m_lazyMutex.unlock();
        m_threadParams[i].m_lazycv.notify_one();
    }
}

void Search::stopWorkerThreads()
{
    volatile int j = 0;

    for (unsigned int i = 0; i < m_thc; ++i) {
        m_threadParams[i].m_smpThreadExit = true;
        m_threadParams[i].m_flags |= TERMINATED_BY_LIMIT;
    }

lbl_retry:
    for (unsigned int i = 0; i < m_thc; ++i)
        while (m_threadParams[i].m_lazyDepth)
            ++j;

    for (unsigned int i = 0; i < m_thc; ++i)
        if (m_threadParams[i].m_lazyDepth)
            goto lbl_retry;
}

Move Search::startSearch(Time time, int depth, EVAL alpha, EVAL beta, Move & ponder)
{
    m_time = time;
    m_t0 = GetProcTime();
    m_flags = (m_time.getTimeMode() == Time::TimeControl::Infinite ? MODE_ANALYZE : MODE_PLAY);
    m_iterPVSize = 0;
    m_nodes = 0;
    m_selDepth = 0;
    m_tbHits = 0;

    memset(&m_pv, 0, sizeof(m_pv));
    m_time.resetAdjustment();

    //
    //  Probe tablebases/tt at root first
    //

    if (m_principalSearcher) {
        m_best = tableBaseRootSearch();

        if (m_best)
            return m_best;

        /*m_best = hashTableRootSearch();

        if (m_best)
            return m_best;*/
    }

    EVAL aspiration = 15;
    EVAL score = alpha;
    bool singleMove = HaveSingleMove(m_position, m_best);

    string result, comment;

    if ((m_principalSearcher) && IsGameOver(m_position, result, comment)) {
        cout << result << " " << comment << endl << endl;
        return m_best;
    }

    bool smpStarted = false;
    bool lazySmpWork = false;

    for (m_depth = depth; m_depth < MAX_PLY; ++m_depth)
    {
        //
        //  Start worker threads if Threads option is configured
        //

        lazySmpWork = (m_thc) && (!smpStarted) && (m_depth > 1);

        if (lazySmpWork) {
            smpStarted = true;
            startWorkerThreads(time);
        }

        //
        //  Make a search
        //

        score = searchRoot(alpha, beta, m_depth);

        //m_time.adjust(beta - alpha > 1, m_depth, score);

        if (m_flags & SEARCH_TERMINATED)
            break;

        U32 dt = GetProcTime() - m_t0;

        //
        //  Update node statistic from all workers
        //

        if (m_principalSearcher) {
            for (unsigned int i = 0; i < m_thc; ++i) {
                m_nodes += m_threadParams[i].m_nodes;
                m_tbHits += m_threadParams[i].m_tbHits;
                m_threadParams[i].m_nodes = 0;
                m_threadParams[i].m_tbHits = 0;
            }
        }

        //
        //  We found so far a better move
        //

        if (score > alpha && score < beta)
        {
            aspiration = 15;
            alpha = score - aspiration;
            beta = score + aspiration;

            memcpy(m_iterPV, m_pv[0], m_pvSize[0] * sizeof(Move));
            m_iterPVSize = m_pvSize[0];

            if (m_iterPVSize && m_pv[0][0])
            {
                m_best = m_pv[0][0];
                m_bestSmpEval = score;

                if (m_pv[0][1])
                    ponder = m_pv[0][1];
                else
                    ponder = 0;
            }

            if (m_principalSearcher)
                PrintPV(m_position, m_depth, m_selDepth, score, m_pv[0], m_pvSize[0], "");

            if (m_time.getTimeMode() == Time::TimeControl::TimeLimit && dt >= m_time.getSoftLimit())
            {
                m_flags |= TERMINATED_BY_LIMIT;

                stringstream ss;
                ss << "Search stopped by stSoft, dt = " << dt;
                Log(ss.str());

                break;
            }

            if ((m_flags & MODE_ANALYZE) == 0) {
                if (singleMove) {
                    m_flags |= TERMINATED_BY_LIMIT;
                    break;
                }
            }
        }
        else if (score <= alpha)
        {
            alpha = -INFINITY_SCORE;
            beta = INFINITY_SCORE;

            if (!(m_flags & MODE_SILENT) && m_principalSearcher)
                PrintPV(m_position, m_depth, m_selDepth, score, m_pv[0], m_pvSize[0], "");
            --m_depth;

            aspiration += aspiration / 4 + 5;
        }
        else if (score >= beta)
        {
            alpha = -INFINITY_SCORE;
            beta = INFINITY_SCORE;

            memcpy(m_iterPV, m_pv[0], m_pvSize[0] * sizeof(Move));
            m_iterPVSize = m_pvSize[0];

            if (m_iterPVSize && m_pv[0][0])
            {
                m_best = m_pv[0][0];
                m_bestSmpEval = score;

                if (m_pv[0][1])
                    ponder = m_pv[0][1];
                else
                    ponder = 0;
            }

            if (!(m_flags & MODE_SILENT) && m_principalSearcher)
                PrintPV(m_position, m_depth, m_selDepth, score, m_pv[0], m_pvSize[0], "");
            --m_depth;
            aspiration += aspiration / 4 + 5;
        }

        //
        //  Check time limits
        //

        dt = GetProcTime() - m_t0;

        if (m_principalSearcher && (dt > 2000))
            cout << "info depth " << m_depth << " time " << dt << " nodes " << m_nodes << " nps " << 1000 * m_nodes / dt << endl;

        if (m_time.getTimeMode() == Time::TimeControl::TimeLimit && dt >= m_time.getSoftLimit())
        {
            m_flags |= TERMINATED_BY_LIMIT;

            stringstream ss;
            ss << "Search stopped by stSoft, dt = " << dt;
            Log(ss.str());

            break;
        }

        if ((m_flags & MODE_ANALYZE) == 0) {
            if (singleMove) {
                m_flags |= TERMINATED_BY_LIMIT;
                break;
            }
        }

        if (m_time.getTimeMode() == Time::TimeControl::DepthLimit && m_depth >= static_cast<int>(m_time.getDepthLimit()))
        {
            m_flags |= TERMINATED_BY_LIMIT;
            break;
        }
    }

    if (m_flags & MODE_ANALYZE)
    {
        while ((m_flags & SEARCH_TERMINATED) == 0)
        {
            string s;
            getline(cin, s);
            ProcessInput(s);
        }
    }

    //
    //  Stop worker threads if necessary
    //

    if (smpStarted)
        stopWorkerThreads();

    return m_best;
}

void Search::setSyzygyDepth(int depth)
{
    m_syzygyDepth = depth;
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
        m_threads[i] = std::thread(&Search::LazySmpSearcher, &m_threadParams[i]);
}

void Search::LazySmpSearcher()
{
    while (!m_terminateSmp)
    {
        int alpha, beta;

        {
            std::unique_lock<std::mutex> lk(m_lazyMutex);
            m_lazycv.wait(lk, std::bind(&Search::getIsLazySmpWork, this));

            if (m_terminateSmp)
                return;

            alpha = m_lazyAlpha;
            beta = m_lazyBeta;
            m_depth = m_lazyDepth;
        }

        Move ponder;
        startSearch(m_time, m_depth, alpha, beta, ponder);
        resetLazySmpWork();
    }
}

void Search::releaseHelperThreads()
{
    for (unsigned int i = 0; i < m_thc; ++i)
    {
        if (m_threads[i].joinable()) {
            m_threadParams[i].m_terminateSmp = true;
            {
                std::unique_lock<std::mutex> lk(m_threadParams[i].m_lazyMutex);
                m_threadParams[i].m_lazyDepth = 1;
            }
            m_threadParams[i].m_lazycv.notify_one();
            m_threads[i].join();
        }
    }
}
