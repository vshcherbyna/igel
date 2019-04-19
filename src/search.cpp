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

#include <algorithm>

const int SORT_HASH         = 7000000;
const int SORT_CAPTURE      = 6000000;
const int SORT_MATE_KILLER  = 5000000;
const int SORT_KILLER       = 4000000;
const int SORT_HISTORY      = 0;

const U8 HASH_ALPHA = 0;
const U8 HASH_EXACT = 1;
const U8 HASH_BETA = 2;

const EVAL SORT_VALUE[14] = { 0, 0, VAL_P, VAL_P, VAL_N, VAL_N, VAL_B, VAL_B, VAL_R, VAL_R, VAL_Q, VAL_Q, VAL_K, VAL_K };

Search::Search():
m_timeCheck(0),
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
}

Search::~Search()
{
    releaseHelperThreads();
}

bool Search::CheckLimits(bool onPv, int depth, EVAL score)
{
    if ((m_smpThreadExit) || (m_flags & SEARCH_TERMINATED))
        return true;

    if (m_time.getTimeMode() == Time::TimeControl::NodesLimit)
    {
        if (m_nodes >= m_time.getNodesLimit())
            m_flags |= TERMINATED_BY_LIMIT;

        return (m_flags & SEARCH_TERMINATED);
    }

    U32 dt = 0;

    if (++m_timeCheck <= 1000)
        return false;

    m_timeCheck = 0;
    dt = GetProcTime() - m_t0;

    if (m_flags & MODE_PLAY)
    {
        //m_time.adjust(onPv, depth, score);
        if (m_time.getHardLimit() > 0 && dt >= m_time.getHardLimit())
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

void Search::setPosition(Position pos)
{
    m_position = pos;
}

EVAL Search::AlphaBetaRoot(EVAL alpha, EVAL beta, int depth)
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

    UpdateSortScores(mvlist, hashMove, ply);
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

    for (size_t i = 0; i < mvSize; ++i)
    {
        if (m_principalSearcher)
            CheckInput();

        if (CheckLimits(onPV, depth, alpha))
            break;

        Move mv = GetNextBest(mvlist, i);
        if (m_position.MakeMove(mv))
        {
            ++m_nodes;
            ++legalMoves;
            m_histTry[mv.To()][mv.Piece()] += depth;

            if ((m_principalSearcher) && ((GetProcTime() - m_t0) > 2000))
                cout << "info depth " << depth << " currmove " << MoveToStrLong(mv) << " currmovenumber " << legalMoves << endl;

            int newDepth = depth - 1;

            //
            //   EXTENSIONS
            //

            newDepth += Extensions(mv, m_position.LastMove(), inCheck, ply, onPV);

            EVAL e;
            if (legalMoves == 1)
                e = -AlphaBeta(-beta, -alpha, newDepth, ply + 1, false);
            else
            {
                e = -AlphaBeta(-alpha - 1, -alpha, newDepth, ply + 1, false);
                if (e > alpha && e < beta)
                    e = -AlphaBeta(-beta, -alpha, newDepth, ply + 1, false);
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

                if (!mv.Captured())
                    m_histSuccess[mv.To()][mv.Piece()] += depth;
            }

            if (alpha >= beta)
            {
                type = HASH_BETA;
                if (!mv.Captured() && !mv.Promotion())
                {
                    if (alpha >= CHECKMATE_SCORE - 50)
                        m_mateKillers[ply] = mv;
                    else
                        m_killers[ply] = mv;
                }
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

EVAL Search::AlphaBeta(EVAL alpha, EVAL beta, int depth, int ply, bool isNull)
{
    if (ply > MAX_PLY - 2)
        return DRAW_SCORE;

    m_pvSize[ply] = 0;

    if (!isNull && m_position.Repetitions() >= 2)
        return DRAW_SCORE;

    if (ply > m_selDepth)
        m_selDepth = ply;

    //
    //   probe hash
    //

    Move hashMove = 0;
    TEntry hEntry;

    if (ProbeHash(hEntry)) {
        if (hEntry.depth() >= depth) {
            EVAL score = hEntry.score();
            if (score > CHECKMATE_SCORE - 50 && score <= CHECKMATE_SCORE)
                score -= ply;
            if (score < -CHECKMATE_SCORE + 50 && score >= -CHECKMATE_SCORE)
                score += ply;

            if (hEntry.type() == HASH_EXACT)
                return score;
            if (hEntry.type() == HASH_ALPHA && score <= alpha)
                return alpha;
            if (hEntry.type() == HASH_BETA && score >= beta)
                return beta;
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
                    type  = HASH_BETA;
                    break;
                case TB_LOSS:
                    score = -TBBASE_SCORE + MAX_PLY + ply;
                    type  = HASH_ALPHA;
                    break;
                default:
                    score = 0;
                    type  = HASH_EXACT;
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

    auto onPV = (beta - alpha > 1);

    if (CheckLimits(onPV, depth, alpha))
        return -INFINITY_SCORE;

    bool inCheck = m_position.InCheck();

    //
    //   qsearch
    //

    if (!inCheck && depth <= 0)
        return AlphaBetaQ(alpha, beta, ply, 0);

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

    EVAL staticEval = Evaluator::evaluate(m_position);

    bool nonPawnMaterial = m_position.NonPawnMaterial();
    static const EVAL MARGIN[4] = { 0, 50, 350, 550 };

    if (nonPawnMaterial && !onPV && !inCheck && depth >= 1 && depth <= 3) {
        if (staticEval <= alpha - MARGIN[depth])
            return AlphaBetaQ(alpha, beta, ply, 0);
        if (staticEval >= beta + MARGIN[depth])
            return beta;
    }

    //
    //   razoring
    //

    if (!onPV && !inCheck && depth <= 1 && staticEval + 325 < alpha)
        return AlphaBetaQ(alpha, beta, ply, 0);

    //
    //   beta pruning
    //

    if (!onPV && !inCheck && depth <= 8 && staticEval - 85 * depth > beta)
        return staticEval;

    //
    //   null move
    //

    if (!isNull && !onPV && !inCheck && depth >= 2 && nonPawnMaterial && staticEval >= beta) {
        int R = 4 + depth / 6 + min(3, (staticEval - beta) / 200);

        m_position.MakeNullMove();
        EVAL nullScore = -AlphaBeta(-beta, -beta + 1, depth - R, ply + 1, true);
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
                auto score = -AlphaBeta(-betaCut, -betaCut + 1, depth - 4, ply + 1, isNull);
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
        AlphaBeta(alpha, beta, depth - 4, ply, isNull);
        if (m_pvSize[ply] > 0)
            hashMove = m_pv[ply][0];
    }

    int legalMoves = 0;
    int quietMoves = 0;
    Move bestMove = 0;
    U8 type = HASH_ALPHA;

    MoveList& mvlist = m_lists[ply];
    if (inCheck)
        GenMovesInCheck(m_position, mvlist);
    else
        GenAllMoves(m_position, mvlist);

    UpdateSortScores(mvlist, hashMove, ply);

    COLOR side = m_position.Side();
    auto lateEndgame = (m_position.MatIndex(side) < 5);

    auto mvSize = mvlist.Size();
    for (size_t i = 0; i < mvSize; ++i) {
        Move mv = GetNextBest(mvlist, i);
        if (m_position.MakeMove(mv))
        {
            ++m_nodes;
            ++legalMoves;
            m_histTry[mv.To()][mv.Piece()] += depth;

            int newDepth = depth - 1;

            //
            //   extensions
            //

            newDepth += Extensions(mv, m_position.LastMove(), inCheck, ply, onPV);

            EVAL e;
            if (legalMoves == 1)
                e = -AlphaBeta(-beta, -alpha, newDepth, ply + 1, false);
            else
            {
                //
                //   lmr
                //

                int reduction = 0;
                if (depth >= 3 &&
                    !onPV &&
                    !inCheck &&
                    !m_position.InCheck() &&
                    !mv.Captured() &&
                    !mv.Promotion() &&
                    mv != m_mateKillers[ply] &&
                    mv != m_killers[ply] &&
                    m_histSuccess[mv.Piece()][mv.To()] <= m_histTry[mv.Piece()][mv.To()] * 3 / 4 &&
                    !lateEndgame)
                {
                    ++quietMoves;
                    if (quietMoves >= 4)
                        reduction = 1;
                }

                e = -AlphaBeta(-alpha - 1, -alpha, newDepth - reduction, ply + 1, false);

                if (e > alpha && reduction > 0)
                    e = -AlphaBeta(-alpha - 1, -alpha, newDepth, ply + 1, false);
                if (e > alpha && e < beta)
                    e = -AlphaBeta(-beta, -alpha, newDepth, ply + 1, false);
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

                if (!mv.Captured())
                    m_histSuccess[mv.To()][mv.Piece()] += depth;
            }

            if (alpha >= beta)
            {
                type = HASH_BETA;
                if (!mv.Captured())
                {
                    if (alpha >= CHECKMATE_SCORE - 50)
                        m_mateKillers[ply] = mv;
                    else
                        m_killers[ply] = mv;
                }
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

EVAL Search::AlphaBetaQ(EVAL alpha, EVAL beta, int ply, int qply)
{
    bool inCheck = m_position.InCheck();

    if (ply > MAX_PLY - 2)
        return DRAW_SCORE;

    m_pvSize[ply] = 0;

    if (!inCheck)
    {
        EVAL staticScore = Evaluator::evaluate(m_position);
        if (staticScore > alpha)
            alpha = staticScore;
        if (alpha >= beta)
            return beta;
    }

    if (CheckLimits((beta - alpha > 1), qply, alpha))
        return -INFINITY_SCORE;

    MoveList& mvlist = m_lists[ply];
    if (inCheck)
        GenMovesInCheck(m_position, mvlist);
    else
    {
        GenCapturesAndPromotions(m_position, mvlist);
        if (qply < 2)
            AddSimpleChecks(m_position, mvlist);
    }
    UpdateSortScoresQ(mvlist, ply);

    int legalMoves = 0;
    auto mvSize = mvlist.Size();
    for (size_t i = 0; i < mvSize; ++i)
    {
        Move mv = GetNextBest(mvlist, i);

        if (!inCheck && SEE(mv) < 0)
            continue;

        if (m_position.MakeMove(mv))
        {
            ++m_nodes;
            ++legalMoves;

            EVAL e = -AlphaBetaQ(-beta, -alpha, ply + 1, qply + 1);
            m_position.UnmakeMove();

            if (m_flags & SEARCH_TERMINATED)
                break;

            if (e > alpha)
            {
                alpha = e;
                m_pv[ply][0] = mv;
                memcpy(m_pv[ply] + 1, m_pv[ply + 1], m_pvSize[ply + 1] * sizeof(Move));
                m_pvSize[ply] = 1 + m_pvSize[ply + 1];
            }
            if (alpha >= beta)
                break;
        }
    }

    if (legalMoves == 0)
    {
        if (inCheck)
            alpha = -CHECKMATE_SCORE + ply;
    }

    return alpha;
}

void Search::clearHistory()
{
    memset(m_histTry, 0, 64 * 14 * sizeof(int));
    memset(m_histSuccess, 0, 64 * 14 * sizeof(int));

    for (unsigned int i = 0; i < m_thc; ++i)
    {
        memset(m_threadParams[i].m_histTry, 0, 64 * 14 * sizeof(int));
        memset(m_threadParams[i].m_histSuccess, 0, 64 * 14 * sizeof(int));
    }
}

void Search::clearKillers()
{
    memset(m_killers, 0, MAX_PLY * sizeof(Move));
    memset(m_mateKillers, 0, MAX_PLY * sizeof(Move));

    for (unsigned int i = 0; i < m_thc; ++i)
    {
        memset(m_threadParams[i].m_killers, 0, MAX_PLY * sizeof(Move));
        memset(m_threadParams[i].m_mateKillers, 0, MAX_PLY * sizeof(Move));
    }
}

int Search::Extensions(Move mv, Move lastMove, bool inCheck, int ply, bool onPV)
{
    if (inCheck)
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

Move Search::GetNextBest(MoveList& mvlist, size_t i)
{
    if (i == 0 && mvlist[0].m_score == SORT_HASH)
        return mvlist[0].m_mv;

    auto mvSize = mvlist.Size();
    for (size_t j = i + 1; j < mvSize; ++j)
    {
        if (mvlist[j].m_score > mvlist[i].m_score)
            swap(mvlist[i], mvlist[j]);
    }
    return mvlist[i].m_mv;
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

bool Search::IsGoodCapture(Move mv)
{
    return SORT_VALUE[mv.Captured()] >= SORT_VALUE[mv.Piece()];
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

EVAL Search::SEE_Exchange(FLD to, COLOR side, EVAL currScore, EVAL target, U64 occ)
{
    U64 att = m_position.GetAttacks(to, side, occ) & occ;
    if (att == 0)
        return currScore;

    FLD from = NF;
    PIECE piece;
    EVAL newTarget = SORT_VALUE[KW] + 1;

    while (att)
    {
        FLD f = PopLSB(att);
        piece = m_position[f];
        if (SORT_VALUE[piece] < newTarget)
        {
            from = f;
            newTarget = SORT_VALUE[piece];
        }
    }

    occ ^= BB_SINGLE[from];
    EVAL score = - SEE_Exchange(to, side ^ 1, -(currScore + target), newTarget, occ);
    return (score > currScore)? score : currScore;
}

EVAL Search::SEE(Move mv)
{
    FLD from = mv.From();
    FLD to = mv.To();
    PIECE piece = mv.Piece();
    PIECE captured = mv.Captured();
    PIECE promotion = mv.Promotion();
    COLOR side = GetColor(piece);

    EVAL score0 = SORT_VALUE[captured];
    if (promotion)
    {
        score0 += SORT_VALUE[promotion] - SORT_VALUE[PW];
        piece = promotion;
    }

    U64 occ = m_position.BitsAll() ^ BB_SINGLE[from];
    EVAL score = - SEE_Exchange(to, side ^ 1, -score0, SORT_VALUE[piece], occ);

    return score;
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
    //  Probe tablebases at root first
    //

    if (m_principalSearcher) {
        m_best = tableBaseRootSearch();

        if (m_best)
            return m_best;
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
        lazySmpWork = (m_thc) && (!smpStarted) && (m_depth > 1);

        if (lazySmpWork)
        {
            smpStarted = true;
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

        score = AlphaBetaRoot(alpha, beta, m_depth);

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

            if (m_time.getSoftLimit() > 0 && dt >= m_time.getSoftLimit())
            {
                m_flags |= TERMINATED_BY_LIMIT;

                stringstream ss;
                ss << "Search stopped by stSoft, dt = " << dt;
                Log(ss.str());

                break;
            }

            if ((m_flags & MODE_ANALYZE) == 0)
            {
                if (singleMove)
                {
                    m_flags |= TERMINATED_BY_LIMIT;
                    break;
                }

                if (score + m_depth >= CHECKMATE_SCORE)
                {
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

        dt = GetProcTime() - m_t0;

        if (m_principalSearcher && (dt > 2000))
            cout << "info depth " << m_depth << " time " << dt << " nodes " << m_nodes << " nps " << 1000 * m_nodes / dt << endl;

        if (m_time.getSoftLimit() > 0 && dt >= m_time.getSoftLimit())
        {
            m_flags |= TERMINATED_BY_LIMIT;

            stringstream ss;
            ss << "Search stopped by stSoft, dt = " << dt;
            Log(ss.str());

            break;
        }

        if ((m_flags & MODE_ANALYZE) == 0)
        {
            if (singleMove)
            {
                m_flags |= TERMINATED_BY_LIMIT;
                break;
            }

            if (score + m_depth >= CHECKMATE_SCORE)
            {
                m_flags |= TERMINATED_BY_LIMIT;
                break;
            }
        }

        if ((m_time.getTimeMode() == Time::TimeControl::DepthLimit) && m_depth >= static_cast<int>(m_time.getDepthLimit()))
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

    if (smpStarted)
    {
        volatile int j = 0;

        for (unsigned int i = 0; i < m_thc; ++i)
            m_threadParams[i].m_smpThreadExit = true;

lbl_retry:
        for (unsigned int i = 0; i < m_thc; ++i)
            while (m_threadParams[i].m_lazyDepth)
                ++j;

        for (unsigned int i = 0; i < m_thc; ++i)
            if (m_threadParams[i].m_lazyDepth)
                goto lbl_retry;
    }

    return m_best;
}

void Search::UpdateSortScores(MoveList& mvlist, Move hashMove, int ply)
{
    auto mvSize = mvlist.Size();
    for (size_t j = 0; j < mvSize; ++j)
    {
        Move mv = mvlist[j].m_mv;
        if (mv == hashMove)
        {
            mvlist[j].m_score = SORT_HASH;
            mvlist.Swap(j, 0);
        }
        else if (mv.Captured() || mv.Promotion())
        {
            EVAL s_piece = SORT_VALUE[mv.Piece()];
            EVAL s_captured = SORT_VALUE[mv.Captured()];
            EVAL s_promotion = SORT_VALUE[mv.Promotion()];

            mvlist[j].m_score = SORT_CAPTURE + 10 * (s_captured + s_promotion) - s_piece;
        }
        else if (mv == m_mateKillers[ply])
            mvlist[j].m_score = SORT_MATE_KILLER;
        else if (mv == m_killers[ply])
            mvlist[j].m_score = SORT_KILLER;
        else
        {
            mvlist[j].m_score = SORT_HISTORY;
            if (m_histTry[mv.To()][mv.Piece()] > 0)
                mvlist[j].m_score += 100 * m_histSuccess[mv.To()][mv.Piece()] / m_histTry[mv.To()][mv.Piece()];
        }
    }
}

void Search::UpdateSortScoresQ(MoveList& mvlist, int ply)
{
    auto mvSize = mvlist.Size();
    for (size_t j = 0; j < mvSize; ++j)
    {
        Move mv = mvlist[j].m_mv;
        if (mv.Captured() || mv.Promotion())
        {
            EVAL s_piece = SORT_VALUE[mv.Piece()];
            EVAL s_captured = SORT_VALUE[mv.Captured()];
            EVAL s_promotion = SORT_VALUE[mv.Promotion()];

            mvlist[j].m_score = SORT_CAPTURE + 10 * (s_captured + s_promotion) - s_piece;
        }
        else if (mv == m_mateKillers[ply])
            mvlist[j].m_score = SORT_MATE_KILLER;
        else if (mv == m_killers[ply])
            mvlist[j].m_score = SORT_KILLER;
        else
        {
            mvlist[j].m_score = SORT_HISTORY;
            if (m_histTry[mv.To()][mv.Piece()] > 0)
                mvlist[j].m_score += 100 * m_histSuccess[mv.To()][mv.Piece()] / m_histTry[mv.To()][mv.Piece()];
        }
    }
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
