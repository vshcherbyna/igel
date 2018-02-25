//   GreKo chess engine
//   (c) 2002-2018 Vladimir Medvedev <vrm@bk.ru>
//   http://greko.su

#include "eval.h"
#include "learn.h"
#include "moves.h"
#include "notation.h"
#include "search.h"
#include "utils.h"

const string PROGRAM_NAME = "igel 1.0";
const string RELEASE_DATE = "Feb-2018";

const int MIN_HASH_SIZE = 1;
const int MAX_HASH_SIZE = 1024;
const int DEFAULT_HASH_SIZE = 128;

extern Pair PSQ[14][64];
extern Pair PSQ_PP_BLOCKED[64];
extern Pair PSQ_PP_FREE[64];

Position g_pos;
deque<string> g_queue;
FILE* g_log = NULL;

bool g_console = false;
bool g_xboard = false;
bool g_uci = true;

static string g_s;
static vector<string> g_tokens;
static bool g_force = false;

void OnZero();

void ComputeTimeLimits(U32 rest, U32 inc)
{
	g_stHard = rest / 2;
	g_stSoft = rest / 100 + g_inc / 2;
}
////////////////////////////////////////////////////////////////////////////////

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
////////////////////////////////////////////////////////////////////////////////

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
////////////////////////////////////////////////////////////////////////////////

void OnEval()
{
	cout << Evaluate(g_pos, -INFINITY_SCORE, INFINITY_SCORE) << endl << endl;
}
////////////////////////////////////////////////////////////////////////////////

void OnFEN()
{
	cout << g_pos.FEN() << endl << endl;
}
////////////////////////////////////////////////////////////////////////////////

void OnFlip()
{
	g_pos.Mirror();
	g_pos.Print();
}
////////////////////////////////////////////////////////////////////////////////

void OnGoUci()
{
	U8 mode = MODE_PLAY;

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
			int rest = -1;
			if (token == "movetime")
			{
				g_sd = 0;
				g_sn = 0;
				int t = atoi(g_tokens[i + 1].c_str());
				g_stHard = t;
				g_stSoft = t;
				++i;
			}
			else if ((token == "wtime" && g_pos.Side() == WHITE) || (token == "btime" && g_pos.Side() == BLACK))
			{
				rest = atoi(g_tokens[i + 1].c_str());
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

			if (rest >= 0)
			{
				g_sd = 0;
				g_sn = 0;
				ComputeTimeLimits(rest, g_inc);

				if (g_log)
				{
					stringstream ss;
					ss << "Time limits: rest = " << rest << ", inc = " << g_inc <<
						" ==> stHard = " << g_stHard << ", stSoft = " << g_stSoft;
					Log(ss.str());
				}
			}
		}
	}

	Move mv = StartSearch(g_pos, mode);
	cout << "bestmove " << MoveToStrLong(mv) << endl;
}
////////////////////////////////////////////////////////////////////////////////

void OnGo()
{
	if (g_uci)
	{
		OnGoUci();
		return;
	}

	g_force = false;

	string result, comment;
	if (IsGameOver(g_pos, result, comment))
	{
		cout << result << " " << comment << endl << endl;
		return;
	}

	Move mv = StartSearch(g_pos, MODE_PLAY);

	if (mv)
	{
		Highlight(true);
		cout << "move " << MoveToStrLong(mv) << endl;
		Highlight(false);

		g_pos.MakeMove(mv);

		if (IsGameOver(g_pos, result, comment))
			cout << result << " " << comment << endl;

		cout << endl;
	}
}
////////////////////////////////////////////////////////////////////////////////

void OnIsready()
{
	cout << "readyok" << endl;
}
////////////////////////////////////////////////////////////////////////////////

void OnLearn(int alg, int limitTimeInSec)
{
	if (g_tokens.size() < 2)
		return;

	string pgnFile, fenFile;

	string fileName = g_tokens[1];
	if (fileName.find(".pgn") != string::npos)
	{
		pgnFile = fileName;
		size_t extPos = fileName.find(".pgn");
		if (extPos != string::npos)
			fenFile = fileName.substr(0, extPos) + ".fen";
	}
	else if (fileName.find(".fen") != string::npos)
	{
		fenFile = fileName;
		size_t extPos = fileName.find(".fen");
		if (extPos != string::npos)
			pgnFile = fileName.substr(0, extPos) + ".pgn";
	}
	else
	{
		pgnFile = fileName + ".pgn";
		fenFile = fileName + ".fen";
	}

	{
		ifstream test(fenFile.c_str());
		if (!test.good())
		{
			if (!PgnToFen(pgnFile, fenFile, 6, 999, 0, 1))
				return;
		}
	}

	if (g_tokens.size() > 2)
		alg = atoi(g_tokens[2].c_str());

	if (g_tokens.size() > 3)
		limitTimeInSec = atoi(g_tokens[3].c_str());

	vector<int> params;
	ifstream f1("learn_params.txt");
	if (f1.good())
	{
		string s;
		while(getline(f1, s))
		{
			vector<string> tokens;
			Split(s, tokens);
			if (tokens.empty()) continue;
			string name = tokens[0];
			for (size_t i = 0; i < NUM_LINES; ++i)
			{
				if (name == lines[i].name)
				{
					for (int j = 0; j < lines[i].len; ++j)
						params.push_back(lines[i].start + j);
				}
			}
		}
	}

	if (params.empty())
	{
		for (int i = 0; i < NUM_PARAMS; ++i)
			params.push_back(i);
	}

	vector<int> x0 = W;
	SetStartLearnTime();

	if (alg == 1)
		CoordinateDescent(fenFile, x0, params);
	else if (alg == 2)
		RandomWalk(fenFile, x0, limitTimeInSec, false, params);
	else if (alg == 3)
		RandomWalk(fenFile, x0, limitTimeInSec, true, params);
	else
		cout << "Unknown algorithm: " << alg << endl << endl;
}
////////////////////////////////////////////////////////////////////////////////

void OnLevel()
{
	if (g_tokens.size() > 3)
		g_inc = 1000 * atoi(g_tokens[3].c_str());
}
////////////////////////////////////////////////////////////////////////////////

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
////////////////////////////////////////////////////////////////////////////////

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
////////////////////////////////////////////////////////////////////////////////

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
////////////////////////////////////////////////////////////////////////////////

void OnNew()
{
	g_force = false;
	g_pos.SetInitial();

	ClearHash();
	ClearHistory();
	ClearKillers();
}
////////////////////////////////////////////////////////////////////////////////

void OnPerft()
{
	if (g_tokens.size() < 2)
		return;
	int depth = atoi(g_tokens[1].c_str());
	StartPerft(g_pos, depth);
}
////////////////////////////////////////////////////////////////////////////////

void OnPgnToFen()
{
	if (g_tokens.size() < 3)
		return;

	string pgnFile = g_tokens[1];
	string fenFile = g_tokens[2];
	int fensPerGame = 1;
	if (g_tokens.size() >= 4)
		fensPerGame = atoi(g_tokens[3].c_str());
	PgnToFen(pgnFile, fenFile, 6, 999, 0, fensPerGame);
}
////////////////////////////////////////////////////////////////////////////////

void OnPing()
{
	if (g_tokens.size() < 2)
		return;
	cout << "pong " << g_tokens[1] << endl;
}
////////////////////////////////////////////////////////////////////////////////

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
////////////////////////////////////////////////////////////////////////////////

void OnPredict()
{
	if (g_tokens.size() < 2)
		return;

	string file = g_tokens[1];
	if (file.find(".fen") == string::npos)
		file += ".fen";

	cout << setprecision(10) << Predict(file) << endl << endl;
}
////////////////////////////////////////////////////////////////////////////////

void OnProtover()
{
	cout << "feature myname=\"" << PROGRAM_NAME <<
		"\" setboard=1 analyze=1 colors=0 san=0 ping=1 name=1 done=1" << endl;
}
////////////////////////////////////////////////////////////////////////////////

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
////////////////////////////////////////////////////////////////////////////////

void OnSD()
{
	if (g_tokens.size() < 2)
		return;

	g_sd = atoi(g_tokens[1].c_str());
	g_sn = 0;
	g_stHard = 0;
	g_stSoft = 0;
}
////////////////////////////////////////////////////////////////////////////////

void OnSelfPlay(int N, U32 limitTimeInSec)
{
	RandSeed(time(0));
	U32 t0 = GetProcTime();

	if (g_tokens.size() > 1)
		N = atoi(g_tokens[1].c_str());

	for (int games = 0; games < N; ++games)
	{
		if (limitTimeInSec > 0 && GetProcTime() - t0 >= limitTimeInSec * 1000)
		{
			cout << "\nTime limit reached\n";
			break;
		}

		Position pos;
		pos.SetInitial();

		string result, comment;
		string pgn, line;

		while (!IsGameOver(pos, result, comment))
		{
			Move mv = (pos.Ply() < 6)? GetRandomMove(pos) : StartSearch(pos, MODE_PLAY | MODE_SILENT);

			if (mv == 0)
			{
				result = "1/2-1/2";
				comment = "{Adjudication: zero move generated}";
				break;
			}

			MoveList mvlist;
			GenAllMoves(pos, mvlist);
			if (pos.Side() == WHITE)
			{
				stringstream ss;
				ss << pos.Ply() / 2 + 1 << ". ";
				line = line + ss.str();
			}
			line = line + MoveToStrShort(mv, pos, mvlist) + string(" ");

			pos.MakeMove(mv);

			if (line.length() > 60 && pos.Side() == WHITE)
			{
				pgn = pgn + "\n" + line;
				line.clear();
			}

			if (pos.Ply() > 600)
			{
				result = "1/2-1/2";
				comment = "{Adjudication: too long}";
				break;
			}
		}

		stringstream header;
		header << "[Event \"Self-play\"]" << endl <<
			"[Date \"" << CurrentDateStr() << "\"]" << endl <<
			"[White \"" << PROGRAM_NAME << "\"]" << endl <<
			"[Black \"" << PROGRAM_NAME << "\"]" << endl <<
			"[Result \"" << result << "\"]" << endl;

		// cout << endl << header.str() << pgn << endl << result << " " << comment << endl << endl;

		if (!line.empty())
		{
			pgn = pgn + "\n" + line;
			line.clear();
		}

		std::ofstream ofs;
		ofs.open("games.pgn", ios::app);
		if (ofs.good())
			ofs << header.str() << pgn << endl << result << " " << comment << endl << endl;

		cout << "Playing games: " << games + 1 << " of " << N << "\r";
	}
	cout << endl;
}
////////////////////////////////////////////////////////////////////////////////

void OnSetboard()
{
	if (g_pos.SetFEN(g_s.c_str() + 9))
		g_pos.Print();
	else
		cout << "Illegal FEN" << endl << endl;
}
////////////////////////////////////////////////////////////////////////////////

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
////////////////////////////////////////////////////////////////////////////////

void OnSN()
{
	if (g_tokens.size() < 2)
		return;

	g_sd = 0;
	g_sn = atoi(g_tokens[1].c_str());
	g_stHard = 0;
	g_stSoft = 0;
}
////////////////////////////////////////////////////////////////////////////////

void OnST()
{
	if (g_tokens.size() < 2)
		return;

	g_sd = 0;
	g_sn = 0;
	g_stHard = U32(1000 * atof(g_tokens[1].c_str()));
	g_stSoft = U32(1000 * atof(g_tokens[1].c_str()));
}
////////////////////////////////////////////////////////////////////////////////

void OnTest()
{
	g_pos.SetFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -");
}
////////////////////////////////////////////////////////////////////////////////

void OnTime()
{
	if (g_tokens.size() < 2)
		return;

	g_sd = 0;
	g_sn = 0;

	int rest = 10 * atoi(g_tokens[1].c_str());
	ComputeTimeLimits(rest, g_inc);

	if (g_log)
	{
		stringstream ss;
		ss << "Time limits: rest = " << rest << ", inc = " << g_inc <<
			" ==> stHard = " << g_stHard << ", stSoft = " << g_stSoft;
		Log(ss.str());
	}
}
////////////////////////////////////////////////////////////////////////////////

void OnTraining()
{
	const string pgn_file = "games.pgn";
	const string fen_file = "games.fen";

	RandSeed(time(0));

	stringstream ss;
	ss << "weights_" << CountGames(pgn_file) << ".txt";
	WriteParamsToFile(W, ss.str());
	cout << "Weights saved in " << ss.str() << endl;

	while (1)
	{
		int skipGames = CountGames(pgn_file);
		cout << "Found " << skipGames << " games in " << pgn_file << endl;

		g_tokens.clear();
		g_tokens.push_back("selfplay");
		OnSelfPlay(10000, 0);

		int N = CountGames(pgn_file);

		if (!PgnToFen(pgn_file, fen_file, 6, 999, skipGames, 1))
			return;

		OnZero();

		g_tokens.clear();
		g_tokens.push_back("learn");
		g_tokens.push_back(fen_file);
		OnLearn(2, 600);

		stringstream ss;
		ss << "weights_" << N << ".txt";
		WriteParamsToFile(W, ss.str());
		cout << "Weights saved in " << ss.str() << endl;
	}
}
////////////////////////////////////////////////////////////////////////////////

void OnUCI()
{
	g_console = false;
	g_xboard = false;
	g_uci = true;

	cout << "id name " << PROGRAM_NAME << endl;
	cout << "id author igel" << endl;

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
////////////////////////////////////////////////////////////////////////////////

void OnXboard()
{
	g_console = false;
	g_xboard = true;
	g_uci = false;

	cout << endl;
}
////////////////////////////////////////////////////////////////////////////////

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
////////////////////////////////////////////////////////////////////////////////

void RunCommandLine()
{
	while (1)
	{
		if (!g_queue.empty())
		{
			g_s = g_queue.front();
			g_queue.pop_front();
		}
		else
		{
			if (g_console)
			{
				if (g_pos.Side() == WHITE)
					cout << "White(" << g_pos.Ply() / 2 + 1 << "): ";
				else
					cout << "Black(" << g_pos.Ply() / 2 + 1 << "): ";
			}
			getline(cin, g_s);
		}

		if (g_s.empty())
			continue;

		Log(string("> ") + g_s);

		Split(g_s, g_tokens);
		if (g_tokens.empty())
			continue;

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
		ON_CMD(go,         1, OnGo())
		ON_CMD(isready,    1, OnIsready())
		ON_CMD(learn,      3, OnLearn(2, 600))
		ON_CMD(level,      3, OnLevel())
		ON_CMD(list,       2, OnList())
		ON_CMD(load,       2, OnLoad())
		ON_CMD(mt,         2, OnMT())
		ON_CMD(new,        1, OnNew())
		ON_CMD(perft,      2, OnPerft())
		ON_CMD(pgntofen,   2, OnPgnToFen())
		ON_CMD(ping,       2, OnPing())
		ON_CMD(position,   2, OnPosition())
		ON_CMD(predict,    3, OnPredict())
		ON_CMD(protover,   3, OnProtover())
		ON_CMD(psq,        2, OnPSQ())
		ON_CMD(quit,       1, break)
		ON_CMD(sd,         2, OnSD())
		ON_CMD(selfplay,   4, OnSelfPlay(10000, 0))
		ON_CMD(sp,         2, OnSelfPlay(10000, 0))
		ON_CMD(setboard,   3, OnSetboard())
		ON_CMD(setoption,  3, OnSetoption())
		ON_CMD(sn,         2, OnSN())
		ON_CMD(st,         2, OnST())
		ON_CMD(test,       2, OnTest())
		ON_CMD(time,       2, OnTime())
		ON_CMD(training,   2, OnTraining())
		ON_CMD(uci,        1, OnUCI())
		ON_CMD(ucinewgame, 4, OnNew())
		ON_CMD(undo,       1, g_pos.UnmakeMove())
		ON_CMD(xboard,     1, OnXboard())
		ON_CMD(zero,       1, OnZero())
#undef ON_CMD

		if (CanBeMove(cmd))
		{
			Move mv = StrToMove(cmd, g_pos);
			if (mv)
			{
				g_pos.MakeMove(mv);
				string result, comment;
				if (IsGameOver(g_pos, result, comment))
					cout << result << " " << comment << endl << endl;
				else if (!g_force)
					OnGo();
				continue;
			}
		}

		if (!g_uci)
			cout << "Unknown command: " << cmd << endl << endl;
	}
}
////////////////////////////////////////////////////////////////////////////////

int main(int argc, const char* argv[])
{
	InitIO();

	if (IsPipe())
	{
		cout << PROGRAM_NAME << " by Vladimir Medvedev" << endl;
		g_uci = true;
		g_xboard = false;
		g_console = false;
	}
	else
	{
		Highlight(true);
		cout << endl << PROGRAM_NAME << " (" << RELEASE_DATE << ")" << endl << endl;
		Highlight(false);
		g_uci = false;
		g_xboard = false;
		g_console = true;
	}

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
			g_log = fopen("GreKo.log", "at");
	}


	SetHashSize(hashMb);

	RunCommandLine();
	return 0;
}
////////////////////////////////////////////////////////////////////////////////
