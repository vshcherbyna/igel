//   GreKo chess engine
//   (c) 2002-2018 Vladimir Medvedev <vrm@bk.ru>
//   http://greko.su

#include <windows.h>
#include <conio.h>
#include "types.h"

static int g_pipe = 0;
static HANDLE g_handle = 0;

U32 GetProcTime()
{
	return GetTickCount();
}
////////////////////////////////////////////////////////////////////////////////

void Highlight(bool on)
{
	WORD intensity = on? FOREGROUND_INTENSITY : 0;

	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
		FOREGROUND_RED |
		FOREGROUND_GREEN |
		FOREGROUND_BLUE |
		intensity);
}
////////////////////////////////////////////////////////////////////////////////

void InitIO()
{
	DWORD dw;
	g_handle = GetStdHandle(STD_INPUT_HANDLE);
	g_pipe = !GetConsoleMode(g_handle, &dw);
	setbuf(stdout, NULL);
}
////////////////////////////////////////////////////////////////////////////////

bool InputAvailable()
{
// TODO: the line below fails VS2017 compilation
//	if (stdin->_cnt > 0)
//		return true;

	if (g_pipe)
	{
		DWORD nchars = 0;
		if (!PeekNamedPipe(g_handle, NULL, 0, NULL, &nchars, NULL))
			return true;
		return (nchars != 0);
	}
	else
		return (_kbhit() != 0);
}
////////////////////////////////////////////////////////////////////////////////

bool IsPipe()
{
	return g_pipe != 0;
}
////////////////////////////////////////////////////////////////////////////////

void SleepMillisec(int msec)
{
	Sleep(msec);
}
////////////////////////////////////////////////////////////////////////////////