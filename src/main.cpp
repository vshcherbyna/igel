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

const string PROGRAM_NAME   = "igel";
const string VERSION        = "1.0";

const int MIN_HASH_SIZE = 1;
const int MAX_HASH_SIZE = 1024;
const int DEFAULT_HASH_SIZE = 128;

extern Pair PSQ[14][64];
extern Pair PSQ_PP_BLOCKED[64];
extern Pair PSQ_PP_FREE[64];

Position g_pos;
deque<string> g_queue;
FILE* g_log = NULL;

static string g_s;
static vector<string> g_tokens;
static bool g_force = false;

void OnZero();

void ComputeTimeLimits(U32 ourTime, U32 enemyTime, U32 inc)
{
    g_stHard = ourTime / 2;

    if (ourTime <= 1000)
    {
        g_stSoft = (ourTime / 100) + (g_inc / 2); // time is running out
    }
    else
    {
        if (ourTime <= enemyTime)
        {
            g_stSoft = (ourTime / 30) + (g_inc / 2);
        }
        else
        {
            U32 diff = ourTime - enemyTime;
            g_stSoft = (ourTime / 30) + (g_inc / 2) + (diff / 100);
        }
    }
}

void OnAnalyze()
{
    g_sd = 0;
    g_sn = 0;
    g_stHard = 0;
    g_stSoft = 0;

    ClearHash();
    ClearHistory();
    ClearKillers();

    Position pos = g_pos;
    StartSearch(pos, MODE_ANALYZE);
}

void OnDump()
{
    ofstream ofs("default_params.cpp");
    if (!ofs.good())
        return;
    ofs << "\tstatic const int data[" << NUM_PARAMS << "] =" << endl;
    ofs << "\t{";
    for (int i = 0; i < NUM_PARAMS; ++i)
    {
        if (i % 50 == 0)
            ofs << endl << "\t\t";
        ofs << W[i];
        if (i < NUM_PARAMS - 1)
        {
            ofs << ",";
            if (i % 50 != 49)
                ofs << " ";
        }
    }
    ofs << endl << "\t};" << endl;
}

void OnEval()
{
    cout << Evaluate(g_pos, -INFINITY_SCORE, INFINITY_SCORE) << endl << endl;
}

void OnFEN()
{
    cout << g_pos.FEN() << endl << endl;
}

void OnFlip()
{
    g_pos.Mirror();
    g_pos.Print();
}

void OnGoUci()
{
    U8 mode = MODE_PLAY;
    int ourTime = -1;
    int enemyTime = -1;

    for (size_t i = 1; i < g_tokens.size(); ++i)
    {
        string token = g_tokens[i];
        if (token == "infinite")
        {
            g_sd = 0;
            g_sn = 0;
            g_stHard = 0;
            g_stSoft = 0;
            mode = MODE_ANALYZE;
        }
        else if (i < g_tokens.size() - 1)
        {
            if (token == "movetime")
            {
                g_sd = 0;
                g_sn = 0;
                int t = atoi(g_tokens[i + 1].c_str());
                g_stHard = t;
                g_stSoft = t;
                ++i;
            }
            else if (token == "wtime")
            {
                if (g_pos.Side() == WHITE)
                    ourTime = atoi(g_tokens[i + 1].c_str());
                else
                    enemyTime = atoi(g_tokens[i + 1].c_str());

                ++i;
            }
            else if (token == "btime")
            {
                if (g_pos.Side() == BLACK)
                    ourTime = atoi(g_tokens[i + 1].c_str());
                else
                    enemyTime = atoi(g_tokens[i + 1].c_str());
                ++i;
            }
            else if ((token == "winc" && g_pos.Side() == WHITE) || (token == "binc" && g_pos.Side() == BLACK))
            {
                g_inc = atoi(g_tokens[i + 1].c_str());
                ++i;
            }
            else if (token == "depth")
            {
                g_sd = atoi(g_tokens[i + 1].c_str());
                g_sn = 0;
                g_stHard = 0;
                g_stSoft = 0;
                ++i;
            }
            else if (token == "nodes")
            {
                g_sd = 0;
                g_sn = atoi(g_tokens[i + 1].c_str());
                g_stHard = 0;
                g_stSoft = 0;
                ++i;
            }

            if (ourTime >= 0 && enemyTime >= 0)
            {
                g_sd = 0;
                g_sn = 0;
                ComputeTimeLimits(ourTime, enemyTime, g_inc);

                if (g_log)
                {
                    stringstream ss;
                    ss << "Time limits: rest = " << ourTime << ", inc = " << g_inc <<
                        " ==> stHard = " << g_stHard << ", stSoft = " << g_stSoft;
                    Log(ss.str());
                }
            }
        }
    }

    Move mv = StartSearch(g_pos, mode);
    cout << "bestmove " << MoveToStrLong(mv) << endl;
}

void OnIsready()
{
    cout << "readyok" << endl;
}

void OnLevel()
{
    if (g_tokens.size() > 3)
        g_inc = 1000 * atoi(g_tokens[3].c_str());
}

void OnList()
{
    MoveList mvlist;
    GenAllMoves(g_pos, mvlist);

    auto mvSize = mvlist.Size();
    for (size_t i = 0; i < mvSize; ++i)
    {
        Move mv = mvlist[i].m_mv;
        cout << MoveToStrLong(mv) << " "; 
    }
    cout << " -- total: " << mvSize << endl << endl;
}

void OnLoad()
{
    if (g_tokens.size() < 2)
        return;

    ifstream ifs(g_tokens[1].c_str());
    if (!ifs.good())
    {
        cout << "Can't open file: " << g_tokens[1] << endl << endl;
        return;
    }

    int line = 1;
    if (g_tokens.size() > 2)
    {
        line = atoi(g_tokens[2].c_str());
        if (line <= 0)
            line = 1;
    }

    string fen;
    for (int i = 0; i < line; ++i)
    {
        if(!getline(ifs, fen)) break;
    }

    if (g_pos.SetFEN(fen))
    {
        g_pos.Print();
        cout << fen << endl << endl;
    }
    else
    {
        cout << "Illegal FEN" << endl << endl;
    }
}

void OnMT()
{
    if (g_tokens.size() < 2)
        return;

    ifstream ifs(g_tokens[1].c_str());
    if (!ifs.good())
    {
        cout << "Can't open file: " << g_tokens[1] << endl << endl;
        return;
    }

    string s;
    while (getline(ifs, s))
    {
        Position pos;
        if (pos.SetFEN(s))
        {
            cout << s << endl;
            EVAL e1 = Evaluate(pos, -INFINITY_SCORE, INFINITY_SCORE);
            pos.Mirror();
            EVAL e2 = Evaluate(pos, -INFINITY_SCORE, INFINITY_SCORE);
            if (e1 != e2)
            {
                pos.Mirror();
                pos.Print();
                cout << "e1 = " << e1 << ", e2 = " << e2 << endl << endl;
                return;
            }
        }
    }

    cout << endl;
}

void OnNew()
{
    g_force = false;
    g_pos.SetInitial();

    ClearHash();
    ClearHistory();
    ClearKillers();
}

void OnPerft()
{
    if (g_tokens.size() < 2)
        return;
    int depth = atoi(g_tokens[1].c_str());
    StartPerft(g_pos, depth);
}

void OnPing()
{
    if (g_tokens.size() < 2)
        return;
    cout << "pong " << g_tokens[1] << endl;
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
        g_pos.SetFEN(fen);
    }
    else if (g_tokens[1] == "startpos")
    {
        g_pos.SetInitial();
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
            Move mv = StrToMove(g_tokens[i], g_pos);
            g_pos.MakeMove(mv);
        }
    }
}

void OnProtover()
{
    cout << "feature myname=\"" << PROGRAM_NAME <<
        "\" setboard=1 analyze=1 colors=0 san=0 ping=1 name=1 done=1" << endl;
}

void OnPSQ()
{
    if (g_tokens.size() < 2)
        return;

    EVAL mid_w = 0;
    EVAL end_w = 0;
    Pair* table = NULL;

    if (g_tokens[1] == "P" || g_tokens[1] == "p")
    {
        table = PSQ[PW];
        mid_w = VAL_P;
        end_w = VAL_P;
    }
    else if (g_tokens[1] == "PPB" || g_tokens[1] == "ppb")
    {
        table = PSQ_PP_BLOCKED;
        mid_w = 0;
        end_w = 0;
    }
    else if (g_tokens[1] == "PPF" || g_tokens[1] == "ppf")
    {
        table = PSQ_PP_FREE;
        mid_w = 0;
        end_w = 0;
    }
    else if (g_tokens[1] == "N" || g_tokens[1] == "n")
    {
        table = PSQ[NW];
        mid_w = VAL_N;
        end_w = VAL_N;
    }
    else if (g_tokens[1] == "B" || g_tokens[1] == "b")
    {
        table = PSQ[BW];
        mid_w = VAL_B;
        end_w = VAL_B;
    }
    else if (g_tokens[1] == "R" || g_tokens[1] == "r")
    {
        table = PSQ[RW];
        mid_w = VAL_R;
        end_w = VAL_R;
    }
    else if (g_tokens[1] == "Q" || g_tokens[1] == "q")
    {
        table = PSQ[QW];
        mid_w = VAL_Q;
        end_w = VAL_Q;
    }
    else if (g_tokens[1] == "K" || g_tokens[1] == "k")
    {
        table = PSQ[KW];
        mid_w = VAL_K;
        end_w = VAL_K;
    }

    if (table != NULL)
    {
        cout << endl << "Midgame:" << endl << endl;
        for (FLD f = 0; f < 64; ++f)
        {
            cout << setw(4) << (table[f].mid - mid_w);
            if (f < 63)
                cout << ", ";
            if (Col(f) == 7)
                cout << endl;
        }

        cout << endl << "Endgame:" << endl << endl;
        for (FLD f = 0; f < 64; ++f)
        {
            cout << setw(4) << (table[f].end - end_w);
            if (f < 63)
                cout << ", ";
            if (Col(f) == 7)
                cout << endl;
        }
        cout << endl;
    }
}

void OnSD()
{
    if (g_tokens.size() < 2)
        return;

    g_sd = atoi(g_tokens[1].c_str());
    g_sn = 0;
    g_stHard = 0;
    g_stSoft = 0;
}

void OnSetboard()
{
    if (g_pos.SetFEN(g_s.c_str() + 9))
        g_pos.Print();
    else
        cout << "Illegal FEN" << endl << endl;
}

void OnSetoption()
{
    if (g_tokens.size() < 5)
        return;
    if (g_tokens[1] != "name" && g_tokens[3] != "value")
        return;

    string name = g_tokens[2];
    string value = g_tokens[4];

    if (name == "Hash")
        SetHashSize(atoi(value.c_str()));
    else if (name == "Strength")
        SetStrength(atoi(value.c_str()));
}

void OnSN()
{
    if (g_tokens.size() < 2)
        return;

    g_sd = 0;
    g_sn = atoi(g_tokens[1].c_str());
    g_stHard = 0;
    g_stSoft = 0;
}

void OnST()
{
    if (g_tokens.size() < 2)
        return;

    g_sd = 0;
    g_sn = 0;
    g_stHard = U32(1000 * atof(g_tokens[1].c_str()));
    g_stSoft = U32(1000 * atof(g_tokens[1].c_str()));
}

void OnTest()
{
    g_pos.SetFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -");
}

void OnTime()
{
    if (g_tokens.size() < 2)
        return;

    g_sd = 0;
    g_sn = 0;

    int rest = 10 * atoi(g_tokens[1].c_str());
    ComputeTimeLimits(rest, 0, g_inc);

    if (g_log)
    {
        stringstream ss;
        ss << "Time limits: rest = " << rest << ", inc = " << g_inc <<
            " ==> stHard = " << g_stHard << ", stSoft = " << g_stSoft;
        Log(ss.str());
    }
}

void OnUCI()
{
    cout << "id name " << PROGRAM_NAME << endl;
    cout << "id author V. Medvedev, V. Shcherbyna" << endl;

    cout << "option name Hash type spin" <<
        " default " << DEFAULT_HASH_SIZE <<
        " min " << MIN_HASH_SIZE <<
        " max " << MAX_HASH_SIZE << endl;

    cout << "option name Strength type spin" <<
        " default " << 100 <<
        " min " << 0 <<
        " max " << 100 << endl;

    cout << "uciok" << endl;
}

void OnZero()
{
    std::vector<int> x;
    x.resize(NUM_PARAMS);
    if (g_tokens.size() > 1 && g_tokens[1].find("rand") == 0)
    {
        for (int i = 0; i < NUM_PARAMS; ++i)
            x[i] = Rand32() % 7 - 3;
    }
    else if (g_tokens.size() > 1 && g_tokens[1].find("mat") == 0)
    {
        SetMaterialOnlyValues(x);
    }
    WriteParamsToFile(x, "weights.txt");
    InitEval(x);
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

        ON_CMD(analyze,    1, OnAnalyze())
        ON_CMD(board,      1, g_pos.Print())
        ON_CMD(dump,       1, OnDump())
        ON_CMD(eval,       2, OnEval())
        ON_CMD(fen,        2, OnFEN())
        ON_CMD(flip,       2, OnFlip())
        ON_CMD(force,      2, g_force = true)
        ON_CMD(go,         1, OnGoUci())
        ON_CMD(isready,    1, OnIsready())
        ON_CMD(level,      3, OnLevel())
        ON_CMD(list,       2, OnList())
        ON_CMD(load,       2, OnLoad())
        ON_CMD(mt,         2, OnMT())
        ON_CMD(new,        1, OnNew())
        ON_CMD(perft,      2, OnPerft())
        ON_CMD(ping,       2, OnPing())
        ON_CMD(position,   2, OnPosition())
        ON_CMD(protover,   3, OnProtover())
        ON_CMD(psq,        2, OnPSQ())
        ON_CMD(quit,       1, break)
        ON_CMD(sd,         2, OnSD())
        ON_CMD(setboard,   3, OnSetboard())
        ON_CMD(setoption,  3, OnSetoption())
        ON_CMD(sn,         2, OnSN())
        ON_CMD(st,         2, OnST())
        ON_CMD(test,       2, OnTest())
        ON_CMD(time,       2, OnTime())
        ON_CMD(uci,        1, OnUCI())
        ON_CMD(ucinewgame, 4, OnNew())
        ON_CMD(undo,       1, g_pos.UnmakeMove())
        ON_CMD(zero,       1, OnZero())
#undef ON_CMD
    }
}

int main(int argc, const char* argv[])
{
    cout << PROGRAM_NAME << " " << VERSION << " by V. Medvedev, V. Shcherbyna" << endl;

    InitIO();
    InitBitboards();
    Position::InitHashNumbers();
    InitEval();
    g_pos.SetInitial();

    double hashMb = DEFAULT_HASH_SIZE;

    for (int i = 1; i < argc; ++i)
    {
        if ((i < argc - 1) && (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-H") || !strcmp(argv[i], "-hash") || !strcmp(argv[i], "-Hash")))
        {
            hashMb = atof(argv[i + 1]);
            if (hashMb < MIN_HASH_SIZE || hashMb > MAX_HASH_SIZE)
                hashMb = DEFAULT_HASH_SIZE;
        }
        else if ((i < argc - 1) && (!strcmp(argv[i], "-s") || !strcmp(argv[i], "-S") || !strcmp(argv[i], "-strength") || !strcmp(argv[i], "-Strength")))
        {
            int level = atoi(argv[i + 1]);
            SetStrength(level);
        }
        else if (!strcmp(argv[i], "-log"))
            g_log = fopen("igel.txt", "at");
    }

    SetHashSize(hashMb);
    RunCommandLine();

    return 0;
}

