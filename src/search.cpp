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

extern Position g_pos;
extern deque<string> g_queue;

static Position s_pos;

U32 g_stHard = 2000;
U32 g_stSoft = 2000;
U32 g_inc = 0;
int g_sd = 0;
NODES g_sn = 0;
U32 g_level = 100;
double g_knps = 0;

static U8           g_flags = 0;
static HashEntry*   g_hash = NULL;
static U8           g_hashAge = 0;
static int          g_hashSize = 0;
static int          g_histTry[64][14];
static int          g_histSuccess[64][14];
static int          g_iter = 0;
static Move         g_iterPV[MAX_PLY];
static int          g_iterPVSize = 0;
static Move         g_killers[MAX_PLY];
static MoveList     g_lists[MAX_PLY];
static Move         g_mateKillers[MAX_PLY];
static NODES        g_nodes = 0;
static Move         g_pv[MAX_PLY][MAX_PLY];
static int          g_pvSize[MAX_PLY];
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

EVAL       AlphaBetaRoot(EVAL alpha, EVAL beta, int depth);
EVAL       AlphaBeta(EVAL alpha, EVAL beta, int depth, int ply, bool isNull);
EVAL       AlphaBetaQ(EVAL alpha, EVAL beta, int ply, int qply);
bool       CheckLimits();
void       CheckInput();
int        Extensions(Move mv, Move lastMove, bool inCheck, int ply, bool onPV);
Move       GetNextBest(MoveList& mvlist, size_t i);
bool       IsGoodCapture(Move mv);
void       ProcessInput(const string& s);
HashEntry* ProbeHash();
void       RecordHash(Move mv, EVAL score, I8 depth, int ply, U8 type);
EVAL       SEE(Move mv);
void       UpdateSortScores(MoveList& mvlist, Move hashMove, int ply);
void       UpdateSortScoresQ(MoveList& mvlist, int ply);

void AdjustSpeed()
{
    if (g_flags & SEARCH_TERMINATED)
        return;

    if (g_knps > 0 && g_iter > 1)
    {
        U32 dt = GetProcTime() - g_t0;
        double expectedTime = g_nodes / g_knps;
        while (dt < expectedTime)
        {
            CheckInput();
            if (CheckLimits())
                return;

            dt = GetProcTime() - g_t0;
        }
    }
}

EVAL AlphaBetaRoot(EVAL alpha, EVAL beta, int depth)
{
    int ply = 0;
    g_pvSize[ply] = 0;

    Move hashMove = 0;
    HashEntry* pEntry = ProbeHash();

    if (pEntry != nullptr)
        hashMove = pEntry->m_mv;

    int legalMoves = 0;
    Move bestMove = 0;
    U8 type = HASH_ALPHA;
    bool inCheck = s_pos.InCheck();
    bool onPV = (beta - alpha > 1);

    MoveList& mvlist = g_lists[ply];
    if (inCheck)
        GenMovesInCheck(s_pos, mvlist);
    else
        GenAllMoves(s_pos, mvlist);
    UpdateSortScores(mvlist, hashMove, ply);

    auto mvSize = mvlist.Size();
    for (size_t i = 0; i < mvSize; ++i)
    {
        Move mv = GetNextBest(mvlist, i);
        if (s_pos.MakeMove(mv))
        {
            ++g_nodes;
            ++legalMoves;
            g_histTry[mv.To()][mv.Piece()] += depth;

            cout << "info depth " << depth << " currmove " << MoveToStrLong(mv) << " currmovenumber " << legalMoves << endl;

            int newDepth = depth - 1;

            //
            //   EXTENSIONS
            //

            newDepth += Extensions(mv, s_pos.LastMove(), inCheck, ply, onPV);

            EVAL e;
            if (legalMoves == 1)
                e = -AlphaBeta(-beta, -alpha, newDepth, ply + 1, false);
            else
            {
                e = -AlphaBeta(-alpha - 1, -alpha, newDepth, ply + 1, false);
                if (e > alpha && e < beta)
                    e = -AlphaBeta(-beta, -alpha, newDepth, ply + 1, false);
            }
            s_pos.UnmakeMove();

            CheckInput();

            if (CheckLimits())
                break;

            if (e > alpha)
            {
                alpha = e;
                bestMove = mv;
                type = HASH_EXACT;

                g_pv[ply][0] = mv;
                memcpy(g_pv[ply] + 1, g_pv[ply + 1], g_pvSize[ply + 1] * sizeof(Move));
                g_pvSize[ply] = 1 + g_pvSize[ply + 1];

                if (!mv.Captured())
                    g_histSuccess[mv.To()][mv.Piece()] += depth;
            }

            if (alpha >= beta)
            {
                type = HASH_BETA;
                if (!mv.Captured() && !mv.Promotion())
                {
                    if (alpha >= CHECKMATE_SCORE - 50)
                        g_mateKillers[ply] = mv;
                    else
                        g_killers[ply] = mv;
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

EVAL AlphaBeta(EVAL alpha, EVAL beta, int depth, int ply, bool isNull)
{
    if (ply > MAX_PLY - 2)
        return DRAW_SCORE;

    g_pvSize[ply] = 0;

    if (!isNull && s_pos.Repetitions() >= 2)
        return DRAW_SCORE;

    COLOR side = s_pos.Side();
    bool inCheck = s_pos.InCheck();
    bool onPV = (beta - alpha > 1);
    bool lateEndgame = (s_pos.MatIndex(side) < 5);

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

    if (CheckLimits())
        return alpha;

    //
    //   QSEARCH
    //

    if (!inCheck && depth <= 0)
        return AlphaBetaQ(alpha, beta, ply, 0);

    //
    //   FUTILITY
    //

    static const EVAL MARGIN[4] = { 0, 50, 350, 550 };
    if (!onPV && !inCheck && depth >= 1 && depth <= 3)
    {
        EVAL score = Evaluate(s_pos, alpha - MARGIN[depth], beta + MARGIN[depth]);
        if (score <= alpha - MARGIN[depth])
            return AlphaBetaQ(alpha, beta, ply, 0);
        if (score >= beta + MARGIN[depth])
            return beta;
    }

    //
    //   NULL MOVE
    //

    int R = 3;
    if (!isNull && !onPV && !inCheck && !lateEndgame && depth >= 2)
    {
        s_pos.MakeNullMove();
        EVAL nullScore = (depth - 1 - R > 0)?
            -AlphaBeta(-beta, -beta + 1, depth - 1 - R, ply + 1, true) :
            -AlphaBetaQ(-beta, -beta + 1, ply + 1, 0);
        s_pos.UnmakeNullMove();
        if (nullScore >= beta)
            return beta;
    }

    //
    //   IID
    //

    if (onPV && hashMove == 0 && depth > 4)
    {
        AlphaBeta(alpha, beta, depth - 4, ply, isNull);
        if (g_pvSize[ply] > 0)
            hashMove = g_pv[ply][0];
    }

    int legalMoves = 0;
    int quietMoves = 0;
    Move bestMove = 0;
    U8 type = HASH_ALPHA;

    MoveList& mvlist = g_lists[ply];
    if (inCheck)
        GenMovesInCheck(s_pos, mvlist);
    else
        GenAllMoves(s_pos, mvlist);
    UpdateSortScores(mvlist, hashMove, ply);

    auto mvSize = mvlist.Size();
    for (size_t i = 0; i < mvSize; ++i)
    {
        Move mv = GetNextBest(mvlist, i);
        if (s_pos.MakeMove(mv))
        {
            ++g_nodes;
            ++legalMoves;
            g_histTry[mv.To()][mv.Piece()] += depth;

            int newDepth = depth - 1;

            //
            //   EXTENSIONS
            //

            newDepth += Extensions(mv, s_pos.LastMove(), inCheck, ply, onPV);

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
                    !s_pos.InCheck() &&
                    !mv.Captured() &&
                    !mv.Promotion() &&
                    mv != g_mateKillers[ply] &&
                    mv != g_killers[ply] &&
                    g_histSuccess[mv.Piece()][mv.To()] <= g_histTry[mv.Piece()][mv.To()] * 3 / 4 &&
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
            s_pos.UnmakeMove();

            if (g_flags & SEARCH_TERMINATED)
                break;

            if (e > alpha)
            {
                alpha = e;
                bestMove = mv;
                type = HASH_EXACT;

                g_pv[ply][0] = mv;
                memcpy(g_pv[ply] + 1, g_pv[ply + 1], g_pvSize[ply + 1] * sizeof(Move));
                g_pvSize[ply] = 1 + g_pvSize[ply + 1];

                if (!mv.Captured())
                    g_histSuccess[mv.To()][mv.Piece()] += depth;
            }

            if (alpha >= beta)
            {
                type = HASH_BETA;
                if (!mv.Captured())
                {
                    if (alpha >= CHECKMATE_SCORE - 50)
                        g_mateKillers[ply] = mv;
                    else
                        g_killers[ply] = mv;
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
        if (s_pos.Fifty() >= 100)
            alpha = DRAW_SCORE;
    }

    RecordHash(bestMove, alpha, depth, ply, type);
    AdjustSpeed();

    return alpha;
}

EVAL AlphaBetaQ(EVAL alpha, EVAL beta, int ply, int qply)
{
    if (ply > MAX_PLY - 2)
        return alpha;

    g_pvSize[ply] = 0;

    bool inCheck = s_pos.InCheck();
    if (!inCheck)
    {
        EVAL staticScore = Evaluate(s_pos, alpha, beta);
        if (staticScore > alpha)
            alpha = staticScore;
        if (alpha >= beta)
            return beta;
    }

    if (CheckLimits())
        return alpha;

    MoveList& mvlist = g_lists[ply];
    if (inCheck)
        GenMovesInCheck(s_pos, mvlist);
    else
    {
        GenCapturesAndPromotions(s_pos, mvlist);
        if (qply < 2)
            AddSimpleChecks(s_pos, mvlist);
    }
    UpdateSortScoresQ(mvlist, ply);

    int legalMoves = 0;
    auto mvSize = mvlist.Size();
    for (size_t i = 0; i < mvSize; ++i)
    {
        Move mv = GetNextBest(mvlist, i);

        if (!inCheck && SEE(mv) < 0)
            continue;

        if (s_pos.MakeMove(mv))
        {
            ++g_nodes;
            ++legalMoves;

            EVAL e = -AlphaBetaQ(-beta, -alpha, ply + 1, qply + 1);
            s_pos.UnmakeMove();

            if (g_flags & SEARCH_TERMINATED)
                break;

            if (e > alpha)
            {
                alpha = e;
                g_pv[ply][0] = mv;
                memcpy(g_pv[ply] + 1, g_pv[ply + 1], g_pvSize[ply + 1] * sizeof(Move));
                g_pvSize[ply] = 1 + g_pvSize[ply + 1];
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

bool CheckLimits()
{
    if (++g_timeCheck <= 1000)
        return false;

    g_timeCheck = 0;

    if (g_iterPVSize == 0)
        return false;

    if (g_flags & SEARCH_TERMINATED)
        return false;

    U32 dt = GetProcTime() - g_t0;
    if (g_flags & MODE_PLAY)
    {
        if (g_stHard > 0 && dt >= g_stHard)
        {
            g_flags |= TERMINATED_BY_LIMIT;

            stringstream ss;
            ss << "Search stopped by stHard, dt = " << dt;
            Log(ss.str());
        }
        if (g_sn > 0 && g_nodes >= g_sn)
            g_flags |= TERMINATED_BY_LIMIT;
    }

    return (g_flags & SEARCH_TERMINATED);
}

void CheckInput()
{
    if (g_iter < 2)
        return;

    if (InputAvailable())
    {
        string s;
        getline(cin, s);
        ProcessInput(s);
    }
}

void ClearHash()
{
    assert(g_hash != NULL);
    assert(g_hashSize > 0);
    memset(g_hash, 0, g_hashSize * sizeof(HashEntry));
}

void ClearHistory()
{
    memset(g_histTry, 0, 64 * 14 * sizeof(int));
    memset(g_histSuccess, 0, 64 * 14 * sizeof(int));
}

void ClearKillers()
{
    memset(g_killers, 0, MAX_PLY * sizeof(Move));
    memset(g_mateKillers, 0, MAX_PLY * sizeof(Move));
}

int Extensions(Move mv, Move lastMove, bool inCheck, int ply, bool onPV)
{
    if (inCheck)
        return 1;
    else if (ply < 2 * g_iter)
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

Move GetNextBest(MoveList& mvlist, size_t i)
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

bool HaveSingleMove(Position& pos, Move & bestMove)
{
    MoveList mvlist;
    GenAllMoves(pos, mvlist);

    int legalMoves = 0;
    EVAL bestScore = 0;
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

bool IsGameOver(Position& pos, string& result, string& comment)
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

bool IsGoodCapture(Move mv)
{
    return SORT_VALUE[mv.Captured()] >= SORT_VALUE[mv.Piece()];
}

NODES Perft(Position& pos, int depth, int ply)
{
    if (depth <= 0)
        return 1;

    NODES total = 0;

    MoveList& mvlist = g_lists[ply];
    GenAllMoves(pos, mvlist);

    auto mvSize = mvlist.Size();
    for (size_t i = 0; i < mvSize; ++i)
    {
        Move mv = mvlist[i].m_mv;
        if (pos.MakeMove(mv))
        {
            total += Perft(pos, depth - 1, ply + 1);
            pos.UnmakeMove();
        }
    }

    return total;
}

void PrintPV(const Position& pos, int iter, EVAL score, const Move* pv, int pvSize, const string& sign)
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

void ProcessInput(const string& s)
{
    vector<string> tokens;
    Split(s, tokens);
    if (tokens.empty())
        return;

    string cmd = tokens[0];

    if (g_flags & MODE_ANALYZE)
    {
        if (CanBeMove(cmd))
        {
            Move mv = StrToMove(cmd, g_pos);
            if (mv)
            {
                g_pos.MakeMove(mv);
                g_queue.push_back("analyze");
                g_flags |= TERMINATED_BY_USER;
            }
        }
        else if (Is(cmd, "undo", 1) && (g_flags & MODE_ANALYZE))
        {
            g_pos.UnmakeMove();
            g_queue.push_back("analyze");
            g_flags |= TERMINATED_BY_USER;
        }
    }

    if (g_flags & MODE_PLAY)
    {
        if (cmd == "?")
            g_flags |= TERMINATED_BY_USER;
        else if (Is(cmd, "result", 1))
        {
            g_iterPVSize = 0;
            g_flags |= TERMINATED_BY_USER;
        }
    }

    if (Is(cmd, "board", 1))
        g_pos.Print();
    else if (Is(cmd, "exit", 1))
        g_flags |= TERMINATED_BY_USER;
    else if (Is(cmd, "quit", 1))
        exit(0);
    else if (Is(cmd, "stop", 1))
        g_flags |= TERMINATED_BY_USER;
}

HashEntry* ProbeHash()
{
    assert(g_hash != NULL);
    assert(g_hashSize > 0);

    U64 hash0 = s_pos.Hash();

    int index = hash0 % g_hashSize;
    HashEntry* pEntry = g_hash + index;

    if (pEntry->m_hash == hash0)
        return pEntry;
    else
        return NULL;
}

void RecordHash(Move mv, EVAL score, I8 depth, int ply, U8 type)
{
    assert(g_hash != NULL);
    assert(g_hashSize > 0);

    if (g_flags & SEARCH_TERMINATED)
        return;

    U64 hash0 = s_pos.Hash();

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

EVAL SEE_Exchange(FLD to, COLOR side, EVAL currScore, EVAL target, U64 occ)
{
    U64 att = s_pos.GetAttacks(to, side, occ) & occ;
    if (att == 0)
        return currScore;

    FLD from = NF;
    PIECE piece;
    EVAL newTarget = SORT_VALUE[KW] + 1;

    while (att)
    {
        FLD f = PopLSB(att);
        piece = s_pos[f];
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

EVAL SEE(Move mv)
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

    U64 occ = s_pos.BitsAll() ^ BB_SINGLE[from];
    EVAL score = - SEE_Exchange(to, side ^ 1, -score0, SORT_VALUE[piece], occ);

    return score;
}

void SetHashSize(double mb)
{
    if (g_hash)
        delete[] g_hash;

    g_hashSize = int(1024 * 1024 * mb / sizeof(HashEntry));
    g_hash = new HashEntry[g_hashSize];
}

void SetStrength(int level)
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

void StartPerft(Position& pos, int depth)
{
    NODES total = 0;
    U32 t0 = GetProcTime();

    MoveList& mvlist = g_lists[0];
    GenAllMoves(pos, mvlist);

    cout << endl;
    auto mvSize = mvlist.Size();
    for (size_t i = 0; i < mvSize; ++i)
    {
        Move mv = mvlist[i].m_mv;
        if (pos.MakeMove(mv))
        {
            NODES delta = Perft(pos, depth - 1, 1);
            total += delta;
            cout << " " << MoveToStrLong(mv) << " - " << delta << endl;
            pos.UnmakeMove();
        }
    }
    U32 t1 = GetProcTime();
    double dt = (t1 - t0) / 1000.;

    cout << endl;
    cout << " Nodes: " << total << endl;
    cout << " Time:  " << dt << endl;
    if (dt > 0) cout << " Knps:  " << total / dt / 1000. << endl;
    cout << endl;
}

Move FirstLegalMove(Position& pos)
{
    MoveList mvlist;
    GenAllMoves(pos, mvlist);

    auto mvSize = mvlist.Size();
    for (size_t i = 0; i < mvSize; ++i)
    {
        Move mv = mvlist[i].m_mv;
        if (s_pos.MakeMove(mv))
        {
            s_pos.UnmakeMove();
            return mv;
        }
    }

    return 0;
}

Move StartSearch(const Position& pos, U8 flags)
{
    if ((g_stHard) && (g_pos.Side() == WHITE) && (g_pos.isInitialPosition()))
    {
        Move moves[5] = { Move(B1, C3, NW), Move(G1, F3, NW), Move(D2, D4, PW), Move(E2, E4, PW), Move(E2, E3, PW) };
        RandSeed(time(0));
        return moves[Rand32() % 4];
    }

    g_t0 = GetProcTime();
    g_nodes = 0;
    g_flags = flags;
    g_iterPVSize = 0;
    s_pos = pos;

    ++g_hashAge;

    EVAL alpha = -INFINITY_SCORE;
    EVAL beta = INFINITY_SCORE;
    EVAL aspiration = 100;
    EVAL score = alpha;
    Move best;
    bool singleMove = HaveSingleMove(s_pos, best);

    string result, comment;

    if (IsGameOver(s_pos, result, comment))
    {
        cout << result << " " << comment << endl << endl;
    }
    else
    {
        for (g_iter = 1; g_iter < MAX_PLY; ++g_iter)
        {
            score = AlphaBetaRoot(alpha, beta, g_iter);

            if (g_flags & SEARCH_TERMINATED)
                break;

            U32 dt = GetProcTime() - g_t0;

            //
            //  We found so far a better move
            //

            if (score > alpha && score < beta)
            {
                alpha = score - (aspiration / 2);
                beta = score + (aspiration / 2);

                memcpy(g_iterPV, g_pv[0], g_pvSize[0] * sizeof(Move));
                g_iterPVSize = g_pvSize[0];

                if (g_iterPVSize)
                    best = g_pv[0][0];

                if (!(flags & MODE_SILENT))
                    PrintPV(pos, g_iter, score, g_pv[0], g_pvSize[0], "");

                if (g_stSoft > 0 && dt >= g_stSoft)
                {
                    g_flags |= TERMINATED_BY_LIMIT;

                    stringstream ss;
                    ss << "Search stopped by stSoft, dt = " << dt;
                    Log(ss.str());

                    break;
                }

                if ((flags & MODE_ANALYZE) == 0)
                {
                    if (singleMove)
                    {
                        g_flags |= TERMINATED_BY_LIMIT;
                        break;
                    }

                    if (score + g_iter >= CHECKMATE_SCORE)
                    {
                        g_flags |= TERMINATED_BY_LIMIT;
                        break;
                    }
                }
            }
            else
            {
                alpha = -INFINITY_SCORE;
                beta = INFINITY_SCORE;
                --g_iter;
            }

            if (dt)
                cout << "info depth " << g_iter << " time " << dt << " nodes " << g_nodes << " nps " << 1000 * g_nodes / dt << endl;

            dt = GetProcTime() - g_t0;
            if (g_stSoft > 0 && dt >= g_stSoft)
            {
                g_flags |= TERMINATED_BY_LIMIT;

                stringstream ss;
                ss << "Search stopped by stSoft, dt = " << dt;
                Log(ss.str());

                break;
            }

            if ((flags & MODE_ANALYZE) == 0)
            {
                if (singleMove)
                {
                    g_flags |= TERMINATED_BY_LIMIT;
                    break;
                }

                if (score + g_iter >= CHECKMATE_SCORE)
                {
                    g_flags |= TERMINATED_BY_LIMIT;
                    break;
                }
            }

            if (g_sd > 0 && g_iter >= g_sd)
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

void UpdateSortScores(MoveList& mvlist, Move hashMove, int ply)
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
        else if (mv == g_mateKillers[ply])
            mvlist[j].m_score = SORT_MATE_KILLER;
        else if (mv == g_killers[ply])
            mvlist[j].m_score = SORT_KILLER;
        else
        {
            mvlist[j].m_score = SORT_HISTORY;
            if (g_histTry[mv.To()][mv.Piece()] > 0)
                mvlist[j].m_score += 100 * g_histSuccess[mv.To()][mv.Piece()] / g_histTry[mv.To()][mv.Piece()];
        }
    }
}

void UpdateSortScoresQ(MoveList& mvlist, int ply)
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
        else if (mv == g_mateKillers[ply])
            mvlist[j].m_score = SORT_MATE_KILLER;
        else if (mv == g_killers[ply])
            mvlist[j].m_score = SORT_KILLER;
        else
        {
            mvlist[j].m_score = SORT_HISTORY;
            if (g_histTry[mv.To()][mv.Piece()] > 0)
                mvlist[j].m_score += 100 * g_histSuccess[mv.To()][mv.Piece()] / g_histTry[mv.To()][mv.Piece()];
        }
    }
}
