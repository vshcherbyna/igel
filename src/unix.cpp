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

#ifndef _MSC_VER

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include "utils.h"

static int g_pipe = 0;

U32 GetProcTime()
{
	return 1000 * clock() / CLOCKS_PER_SEC;
}

void SleepMillisec(int msec)
{
	usleep(1000 * msec);
}

void InitIO()
{
	g_pipe = !isatty(0);
	if (g_pipe) signal(SIGINT, SIG_IGN);
	setbuf(stdout, NULL);
	setbuf(stdin, NULL);
}

bool InputAvailable()
{
	static fd_set input_fd_set;
	static fd_set except_fd_set;

	FD_ZERO (&input_fd_set);
	FD_ZERO (&except_fd_set);
	FD_SET  (0, &input_fd_set);
	FD_SET  (1, &except_fd_set);

	static struct timeval timeout;
	timeout.tv_sec = timeout.tv_usec = 0;
	static int max_fd = 2;
	// XXX -- track exceptions (in the select(2) sense) here?

	int retval = select(max_fd, &input_fd_set, NULL, &except_fd_set, &timeout);

	if (retval < 0)  // Error occured.
		return false;

	if (retval == 0)  // timeout.
		return false;

	if (FD_ISSET (0, &input_fd_set)) // There is input
		return true;

	if (FD_ISSET (1, &except_fd_set)) // Exception on write,
		exit (1);                       // probably, connection closed.

	return 0;
}

#endif
