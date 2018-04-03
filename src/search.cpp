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

#include "eval.h"
#include "moves.h"
#include "notation.h"
#include "search.h"
#include "utils.h"
#include "time.h"

extern deque<string> g_queue;

U32 g_level = 100;
double g_knps = 0;

static U8           g_flags = 0;
static HashEntry*   g_hash = NULL;
static U8           g_hashAge = 0;
static int          g_hashSize = 0;
static NODES        g_nodes = 0;
static U32          g_t0 = 0;
static NODES        g_timeCheck = 0;

const int SORT_HASH         = 7000000;
const int SORT_CAPTURE      = 6000000;
const int SORT_MATE_KILLER  = 5000000;
const int SORT_KILLER       = 4000000;
const int SORT_HISTORY      = 0;

const U8 HASH_ALPHA = 0;
const U8 HASH_EXACT = 1;
const U8 HASH_BETA = 2;

const EVAL SORT_VALUE[14] = { 0, 0, VAL_P, VAL_P, VAL_N, VAL_N, VAL_B, VAL_B, VAL_R, VAL_R, VAL_Q, VAL_Q, VAL_K, VAL_K };

bool CheckLimits()
{
    if (g_flags & SEARCH_TERMINATED)
        return true;

    if (Time::instance().getTimeMode() == Time::TimeControl::NodesLimit)
    {
        if (g_nodes >= Time::instance().getNodesLimit())
            g_flags |= TERMINATED_BY_LIMIT;

        return (g_flags & SEARCH_TERMINATED);
    }

    U32 dt = 0;

    if (++g_timeCheck <= 1000)
        return false;

    g_timeCheck = 0;
    dt = GetProcTime() - g_t0;

    if (g_flags & MODE_PLAY)
    {
        if (Time::instance().getHardLimit() > 0 && dt >= Time::instance().getHardLimit())
        {
            g_flags |= TERMINATED_BY_LIMIT;

            stringstream ss;
            ss << "Search stopped by stHard, dt = " << dt;
            Log(ss.str());
        }
    }

    return (g_flags & SEARCH_TERMINATED);
}

void ProcessInput(const string& s)
{
    vector<string> tokens;
    Split(s, tokens);
    if (tokens.empty())
        return;

    string cmd = tokens[0];

    if (g_flags & MODE_PLAY)
    {
        if (cmd == "?")
            g_flags |= TERMINATED_BY_USER;
        else if (Is(cmd, "result", 1))
        {
            //m_iterPVSize = 0;
            g_flags |= TERMINATED_BY_USER;
        }
    }

    if (Is(cmd, "exit", 1))
        g_flags |= TERMINATED_BY_USER;
    else if (Is(cmd, "quit", 1))
        exit(0);
    else if (Is(cmd, "stop", 1))
        g_flags |= TERMINATED_BY_USER;
}

void CheckInput()
{
    if (InputAvailable())
    {
        string s;
        getline(cin, s);
        ::ProcessInput(s);
    }
}

void AdjustSpeed()
{
    if (g_flags & SEARCH_TERMINATED || !g_knps)
        return;

    U32 dt = GetProcTime() - g_t0;
    double expectedTime = g_nodes / g_knps;

    while (dt < expectedTime)
    {
        ::CheckInput();
        if (::CheckLimits())
            return;
        dt = GetProcTime() - g_t0;
    }
}

Search::Search(): m_depth(0), m_iterPVSize(0)
{

}

void Search::setPosition(Position pos)
{
    m_position = pos;
}

EVAL Search::AlphaBetaRoot(EVAL alpha, EVAL beta, int depth)
{
    int ply = 0;
    m_pvSize[ply] = 0;

    Move hashMove = 0;
    HashEntry* pEntry = ProbeHash();

    if (pEntry != nullptr)
        hashMove = pEntry->m_mv;

    int legalMoves = 0;
    Move bestMove = 0;
    U8 type = HASH_ALPHA;
    bool inCheck = m_position.InCheck();
    bool onPV = (beta - alpha > 1);

    MoveList& mvlist = m_lists[ply];
    if (inCheck)
        GenMovesInCheck(m_position, mvlist);
    else
        GenAllMoves(m_position, mvlist);
    UpdateSortScores(mvlist, hashMove, ply);

    auto mvSize = mvlist.Size();
    for (size_t i = 0; i < mvSize; ++i)
    {
        ::CheckInput();

        if (::CheckLimits())
            break;

        Move mv = GetNextBest(mvlist, i);
        if (m_position.MakeMove(mv))
        {
            ++g_nodes;
            ++legalMoves;
            m_histTry[mv.To()][mv.Piece()] += depth;

            if ((GetProcTime() - g_t0) > 2000)
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

    RecordHash(bestMove, alpha, depth, ply, type);
    AdjustSpeed();

    return alpha;
}

EVAL Search::AlphaBeta(EVAL alpha, EVAL beta, int depth, int ply, bool isNull)
{
    if (ply > MAX_PLY - 2)
        return DRAW_SCORE;

    m_pvSize[ply] = 0;

    if (!isNull && m_position.Repetitions() >= 2)
        return DRAW_SCORE;

    COLOR side = m_position.Side();
    bool inCheck = m_position.InCheck();
    bool onPV = (beta - alpha > 1);
    bool lateEndgame = (m_position.MatIndex(side) < 5);

    //
    //   PROBING HASH
    //

    Move hashMove = 0;
    HashEntry* pEntry = ProbeHash();
    if (pEntry != NULL)
    {
        hashMove = pEntry->m_mv;

        if (pEntry->m_depth >= depth)
        {
            EVAL score = pEntry->m_score;
            if (score > CHECKMATE_SCORE - 50 && score <= CHECKMATE_SCORE)
                score -= ply;
            if (score < -CHECKMATE_SCORE + 50 && score >= -CHECKMATE_SCORE)
                score += ply;

            if (pEntry->m_type == HASH_EXACT)
                return score;
            if (pEntry->m_type == HASH_ALPHA && score <= alpha)
                return alpha;
            if (pEntry->m_type == HASH_BETA && score >= beta)
                return beta;
        }
    }

    if (::CheckLimits())
        return alpha;

    //
    //   QSEARCH
    //

    if (!inCheck && depth <= 0)
        return AlphaBetaQ(alpha, beta, ply, 0);

    //
    //   FUTILITY
    //

    bool nonPawnMaterial = m_position.NonPawnMaterial();

    static const EVAL MARGIN[4] = { 0, 50, 350, 550 };
    if (nonPawnMaterial && !onPV && !inCheck && depth >= 1 && depth <= 3)
    {
        EVAL score = Evaluate(m_position, alpha - MARGIN[depth], beta + MARGIN[depth]);
        if (score <= alpha - MARGIN[depth])
            return AlphaBetaQ(alpha, beta, ply, 0);
        if (score >= beta + MARGIN[depth])
            return beta;
    }

    //
    //   RAZORING
    //

    if (!onPV && !inCheck && depth <= 2)
    {
        if ((Evaluate(m_position, -INFINITY_SCORE, INFINITY_SCORE) + 200) < beta)
        {
            EVAL score = AlphaBetaQ(alpha, beta, ply, 0);

            if (score < beta)
                return score;
        }
    }

    //
    //   NULL MOVE
    //

    int R = depth / 4 + 3;
    if (!isNull && !onPV && !inCheck && depth >= 2 && nonPawnMaterial)
    {
        m_position.MakeNullMove();
        EVAL nullScore = (depth - 1 - R > 0)?
            -AlphaBeta(-beta, -beta + 1, depth - 1 - R, ply + 1, true) :
            -AlphaBetaQ(-beta, -beta + 1, ply + 1, 0);
        m_position.UnmakeNullMove();

        if (nullScore >= beta)
        {
            if (nullScore >= (CHECKMATE_SCORE - 50))
                return beta;
            else
                return nullScore;
        }
    }

    //
    //   IID
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

    auto mvSize = mvlist.Size();
    for (size_t i = 0; i < mvSize; ++i)
    {
        Move mv = GetNextBest(mvlist, i);
        if (m_position.MakeMove(mv))
        {
            ++g_nodes;
            ++legalMoves;
            m_histTry[mv.To()][mv.Piece()] += depth;

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
                //
                //   LMR
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

            if (g_flags & SEARCH_TERMINATED)
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

    if (legalMoves == 0)
    {
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

    RecordHash(bestMove, alpha, depth, ply, type);
    AdjustSpeed();

    return alpha;
}

EVAL Search::AlphaBetaQ(EVAL alpha, EVAL beta, int ply, int qply)
{
    if (ply > MAX_PLY - 2)
        return alpha;

    m_pvSize[ply] = 0;

    bool inCheck = m_position.InCheck();
    if (!inCheck)
    {
        EVAL staticScore = Evaluate(m_position, alpha, beta);
        if (staticScore > alpha)
            alpha = staticScore;
        if (alpha >= beta)
            return beta;
    }

    if (::CheckLimits())
        return alpha;

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
            ++g_nodes;
            ++legalMoves;

            EVAL e = -AlphaBetaQ(-beta, -alpha, ply + 1, qply + 1);
            m_position.UnmakeMove();

            if (g_flags & SEARCH_TERMINATED)
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

void Search::ClearHash()
{
    assert(g_hash != NULL);
    assert(g_hashSize > 0);
    memset(g_hash, 0, g_hashSize * sizeof(HashEntry));
}

void Search::ClearHistory()
{
    memset(m_histTry, 0, 64 * 14 * sizeof(int));
    memset(m_histSuccess, 0, 64 * 14 * sizeof(int));
}

void Search::ClearKillers()
{
    memset(m_killers, 0, MAX_PLY * sizeof(Move));
    memset(m_mateKillers, 0, MAX_PLY * sizeof(Move));
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
            EVAL score = Evaluate(pos, -INFINITY_SCORE, INFINITY_SCORE);
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

void Search::PrintPV(const Position& pos, int iter, EVAL score, const Move* pv, int pvSize, const string& sign)
{
    if (pvSize == 0)
        return;

    U32 dt = GetProcTime() - g_t0;

    cout << "info depth " << iter;
    if (abs(score) >= (CHECKMATE_SCORE - MAX_PLY))
        cout << " score mate" << ((score >= 0) ? " " : " -") << ((CHECKMATE_SCORE - abs(score)) / 2) + 1;
    else
        cout << " score cp " << score;
    cout << " time " << dt;
    cout << " nodes " << g_nodes;
    if (pvSize > 0)
    {
        cout << " pv";
        for (int i = 0; i < pvSize; ++i)
            cout << " " << MoveToStrLong(pv[i]);
    }
    cout << endl;
}

HashEntry* Search::ProbeHash()
{
    assert(g_hash != NULL);
    assert(g_hashSize > 0);

    U64 hash0 = m_position.Hash();

    int index = hash0 % g_hashSize;
    HashEntry* pEntry = g_hash + index;

    if (pEntry->m_hash == hash0)
        return pEntry;
    else
        return NULL;
}

void Search::RecordHash(Move mv, EVAL score, I8 depth, int ply, U8 type)
{
    assert(g_hash != NULL);
    assert(g_hashSize > 0);

    if (g_flags & SEARCH_TERMINATED)
        return;

    U64 hash0 = m_position.Hash();

    int index = hash0 % g_hashSize;
    HashEntry& entry = g_hash[index];

    if (depth >= entry.m_depth || g_hashAge != entry.m_age)
    {
        entry.m_hash = hash0;
        entry.m_mv = mv;

        if (score > CHECKMATE_SCORE - 50 && score <= CHECKMATE_SCORE)
            score += ply;
        if (score < -CHECKMATE_SCORE + 50 && score >= -CHECKMATE_SCORE)
            score -= ply;

        entry.m_score = score;
        entry.m_depth = depth;
        entry.m_type = type;
        entry.m_age = g_hashAge;
    }
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

void Search::SetHashSize(double mb)
{
    if (g_hash)
        delete[] g_hash;

    g_hashSize = int(1024 * 1024 * mb / sizeof(HashEntry));
    g_hash = new HashEntry[g_hashSize];
}

void Search::SetStrength(int level)
{
    if (level < 0)
        level = 0;
    if (level > 100)
        level = 100;

    g_level = level;

    if (level < 100)
    {
        double r = level / 20.;      // 0 - 5
        g_knps = 0.1 * pow(10, r);   // 100 nps - 10000 knps
    }
    else
        g_knps = 0;
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

Move Search::StartSearch()
{
    if ((Time::instance().getHardLimit()) && (m_position.Side() == WHITE) && (m_position.isInitialPosition()))
    {
        Move moves[5] = { Move(B1, C3, NW), Move(G1, F3, NW), Move(D2, D4, PW), Move(E2, E4, PW), Move(E2, E3, PW) };
        RandSeed(time(0));
        return moves[Rand32() % 4];
    }

    auto timeMode = Time::instance().getTimeMode();
    g_t0 = GetProcTime();
    g_nodes = 0;
    g_flags = (timeMode == Time::TimeControl::Infinite ? MODE_ANALYZE : MODE_PLAY);
    m_iterPVSize = 0;

    ++g_hashAge;

    EVAL alpha = -INFINITY_SCORE;
    EVAL beta = INFINITY_SCORE;
    EVAL aspiration = 15;
    EVAL score = alpha;
    Move best;
    bool singleMove = HaveSingleMove(m_position, best);

    string result, comment;

    if (IsGameOver(m_position, result, comment))
    {
        cout << result << " " << comment << endl << endl;
    }
    else
    {
        for (m_depth = 1; m_depth < MAX_PLY; ++m_depth)
        {
            score = AlphaBetaRoot(alpha, beta, m_depth);

            if (g_flags & SEARCH_TERMINATED)
                break;

            U32 dt = GetProcTime() - g_t0;

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
                    best = m_pv[0][0];

                PrintPV(m_position, m_depth, score, m_pv[0], m_pvSize[0], "");

                if (Time::instance().getSoftLimit() > 0 && dt >= Time::instance().getSoftLimit())
                {
                    g_flags |= TERMINATED_BY_LIMIT;

                    stringstream ss;
                    ss << "Search stopped by stSoft, dt = " << dt;
                    Log(ss.str());

                    break;
                }

                if ((g_flags & MODE_ANALYZE) == 0)
                {
                    if (singleMove)
                    {
                        g_flags |= TERMINATED_BY_LIMIT;
                        break;
                    }

                    if (score + m_depth >= CHECKMATE_SCORE)
                    {
                        g_flags |= TERMINATED_BY_LIMIT;
                        break;
                    }
                }
            }
            else if (score <= alpha)
            {
                alpha = -INFINITY_SCORE;
                beta = INFINITY_SCORE;

                if (!(g_flags & MODE_SILENT))
                    PrintPV(m_position, m_depth, score, m_pv[0], m_pvSize[0], "");
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
                    best = m_pv[0][0];

                if (!(g_flags & MODE_SILENT))
                    PrintPV(m_position, m_depth, score, m_pv[0], m_pvSize[0], "");
                --m_depth;
                aspiration += aspiration / 4 + 5;
            }

            dt = GetProcTime() - g_t0;

            if (dt > 2000)
                cout << "info depth " << m_depth << " time " << dt << " nodes " << g_nodes << " nps " << 1000 * g_nodes / dt << endl;

            if (Time::instance().getSoftLimit() > 0 && dt >= Time::instance().getSoftLimit())
            {
                g_flags |= TERMINATED_BY_LIMIT;

                stringstream ss;
                ss << "Search stopped by stSoft, dt = " << dt;
                Log(ss.str());

                break;
            }

            if ((g_flags & MODE_ANALYZE) == 0)
            {
                if (singleMove)
                {
                    g_flags |= TERMINATED_BY_LIMIT;
                    break;
                }

                if (score + m_depth >= CHECKMATE_SCORE)
                {
                    g_flags |= TERMINATED_BY_LIMIT;
                    break;
                }
            }

            if ((timeMode == Time::TimeControl::DepthLimit) && m_depth >= static_cast<int>(Time::instance().getDepthLimit()))
            {
                g_flags |= TERMINATED_BY_LIMIT;
                break;
            }
        }
    }

    if (g_flags & MODE_ANALYZE)
    {
        while ((g_flags & SEARCH_TERMINATED) == 0)
        {
            string s;
            getline(cin, s);
            ProcessInput(s);
        }
    }

    return best;
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
