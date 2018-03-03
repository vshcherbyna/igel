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

#include <windows.h>
#include <conio.h>
#include "types.h"

static int g_pipe = 0;
static HANDLE g_handle = 0;

U32 GetProcTime()
{
	return GetTickCount();
}

void InitIO()
{
	DWORD dw;
	g_handle = GetStdHandle(STD_INPUT_HANDLE);
	g_pipe = !GetConsoleMode(g_handle, &dw);
	setbuf(stdout, NULL);
}

bool InputAvailable()
{
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
