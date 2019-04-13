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
#include "time.h"
#include "tune.h"
#include "tt.h"
#include "fathom/tbprobe.h"

// Check windows
#if _WIN32 || _WIN64
#if _WIN64
#define ENV64BIT
#else
#define ENV32BIT
#endif
#endif

// Check GCC
#if __GNUC__
#if __x86_64__ || __ppc64__
#define ENV64BIT
#else
#define ENV32BIT
#endif
#endif

const string PROGRAM_NAME   = "Igel";
const string VERSION        = "1.4.3 (2)";

const int MIN_HASH_SIZE     = 1;
const int MAX_HASH_SIZE     = 131072;
const int DEFAULT_HASH_SIZE = 16;

const int MIN_THREADS       = 1;
const int MAX_THREADS       = 128;
const int DEFAULT_THREADS   = 1;

Search g_search;
deque<string> g_queue;
FILE* g_log = nullptr;

static string g_s;
static vector<string> g_tokens;

void OnEval()
{
    cout << Evaluator::evaluate(g_search.m_position) << endl;
}

void OnFEN()
{
    cout << g_search.m_position.FEN() << endl;
}

void OnGoUci()
{
    Time time;

    if (time.parseTime(g_tokens, g_search.m_position.Side() == WHITE) == false)
        cout << "Error: unable to parse command line" << endl;

    TTable::instance().increaseAge();
    g_search.m_principalSearcher = true;

    Move ponder;
    Move mv = g_search.startSearch(time, 1, -INFINITY_SCORE, INFINITY_SCORE, ponder);

    cout << "bestmove " << MoveToStrLong(mv);
    if (ponder)
        cout << " ponder " << MoveToStrLong(ponder);
    cout << endl;
}

void OnIsready()
{
    cout << "readyok" << endl;
}

void OnNew()
{
    g_search.m_position.SetInitial();

    TTable::instance().clearHash();
    TTable::instance().clearAge();

    g_search.clearHistory();
    g_search.clearKillers();
}

void OnPosition()
{
    if (g_tokens.size() < 2)
        return;

    size_t movesTag = 0;
    if (g_tokens[1] == "fen")
    {
        string fen = "";
        for (size_t i = 2; i < g_tokens.size(); ++i)
        {
            if (g_tokens[i] == "moves")
            {
                movesTag = i;
                break;
            }
            if (!fen.empty())
                fen += " ";
            fen += g_tokens[i];
        }
        g_search.m_position.SetFEN(fen);
    }
    else if (g_tokens[1] == "startpos")
    {
        g_search.m_position.SetInitial();
        for (size_t i = 2; i < g_tokens.size(); ++i)
        {
            if (g_tokens[i] == "moves")
            {
                movesTag = i;
                break;
            }
        }
    }

    if (movesTag)
    {
        for (size_t i = movesTag + 1; i < g_tokens.size(); ++i)
        {
            Move mv = StrToMove(g_tokens[i], g_search.m_position);
            g_search.m_position.MakeMove(mv);
        }
    }
}

void OnSetoption()
{
    if (g_tokens.size() < 5)
        return;

    if (g_tokens[1] != "name" && g_tokens[3] != "value")
        return;

    auto name = g_tokens[2];
    auto value = g_tokens[4];

    if (name == "Hash")
    {
        if (!TTable::instance().setHashSize(atoi(value.c_str())))
        {
            cout << "Unable to allocate memory for transposition table" << endl;
            exit(1);
        }
    }
    else if (name == "Threads")
    {
        auto threads = atoi(value.c_str());

        if (threads > MAX_THREADS || threads < MIN_THREADS)
            cout << "Unable set threads value. Make sure number is correct" << endl;
        g_search.setThreadCount(threads - 1);
    }
    else if (name == "SyzygyPath")
    {
        tb_init(value.c_str());
    }
    else if (name == "SyzygyProbeDepth")
    {
        g_search.setSyzygyDepth(atoi(value.c_str()));
    }
    else
        cout << "Unknown option " << name << endl;
}

void OnUCI()
{
    cout << "id name " << PROGRAM_NAME << " " << VERSION;
#if defined(ENV64BIT)
    cout << " 64 POPCNT";
#endif
    cout << endl << "id author V. Medvedev, V. Shcherbyna" << endl;

    cout << "option name Hash type spin" <<
        " default " << DEFAULT_HASH_SIZE <<
        " min " << MIN_HASH_SIZE <<
        " max " << MAX_HASH_SIZE << endl;

    cout << "option name Threads type spin" <<
            " default " << DEFAULT_THREADS <<
            " min " << MIN_THREADS <<
            " max " << MAX_THREADS << endl;

    cout << "option name SyzygyPath type string default <empty>" << endl;

    cout << "option name SyzygyProbeDepth type spin" <<
        " default " << 1 <<
        " min " << 1 <<
        " max " << MAX_PLY << endl;

    cout << "uciok" << endl;
}

NODES Perft(Position & pos, int depth)
{
    if (depth == 0)
        return 1;

    NODES total = 0;
    MoveList mvlist;

    GenAllMoves(pos, mvlist);

    auto mvSize = mvlist.Size();
    for (size_t i = 0; i < mvSize; ++i)
    {
        Move mv = mvlist[i].m_mv;

        if (pos.MakeMove(mv))
        {
            total += Perft(pos, depth - 1);
            pos.UnmakeMove();
        }
    }

    return total;
}

void OnBench()
{
    Position pos;
    pos.SetInitial();

    auto start = GetProcTime();
    auto nodes = Perft(pos, 6);
    auto end = GetProcTime();

    cout << "Total time (ms)\t: " << (end - start) << endl;
    cout << "Nodes searched\t: " << nodes << endl;
    cout << "Nodes/second\t: " << nodes / ((end - start) / 1000) << endl;
}

void RunCommandLine()
{
    while (1)
    {
        getline(cin, g_s);
        Split(g_s, g_tokens);
        if (g_tokens.empty())
            break;

        string cmd = g_tokens[0];

#define ON_CMD(pattern, minLen, action) \
    if (Is(cmd, #pattern, minLen))      \
    {                                   \
      action;                           \
      continue;                         \
    }

        ON_CMD(board,      1, g_search.m_position.Print())
        ON_CMD(eval,       2, OnEval())
        ON_CMD(fen,        2, OnFEN())
        ON_CMD(go,         1, OnGoUci())
        ON_CMD(isready,    1, OnIsready())
        ON_CMD(position,   2, OnPosition())
        ON_CMD(quit,       1, break)
        ON_CMD(setoption,  3, OnSetoption())
        ON_CMD(uci,        1, OnUCI())
        ON_CMD(ucinewgame, 4, OnNew())
        ON_CMD(bench,      1, OnBench())
        ON_CMD(tune,       1, onTune())
#undef ON_CMD
    }
}

#if !defined(UNIT_TEST)
int main(int argc, const char* argv[])
{
    cout << PROGRAM_NAME << " " << VERSION << " by V. Medvedev, V. Shcherbyna" << endl;

    InitIO();
    InitBitboards();
    Position::InitHashNumbers();
    Evaluator::initEval();
    g_search.m_position.SetInitial();
    g_search.setSyzygyDepth(1);

    double hashMb = DEFAULT_HASH_SIZE;

    for (int i = 1; i < argc; ++i)
    {
        if ((i < argc - 1) && (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-H") || !strcmp(argv[i], "-hash") || !strcmp(argv[i], "-Hash")))
        {
            hashMb = atof(argv[i + 1]);
            if (hashMb < MIN_HASH_SIZE || hashMb > MAX_HASH_SIZE)
                hashMb = DEFAULT_HASH_SIZE;
        }
        else if (!strcmp(argv[i], "-log"))
            g_log = fopen("igel.txt", "at");
    }

    if (!TTable::instance().setHashSize(hashMb))
    {
        cout << "Unable to allocate memory for transposition table" << endl;
        return 1;
    }

    RunCommandLine();
    return 0;
}
#endif
