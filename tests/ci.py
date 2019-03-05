#
#  Igel - a UCI chess playing engine derived from GreKo 2018.01
#
#  Copyright (C) 2018-2019 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
#
#  Igel is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  Igel is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with Igel.  If not, see <http://www.gnu.org/licenses/>.
#

#!/usr/bin/python3

import os
import platform

if platform.system() == 'Windows':
    if os.system('..\\unit.exe') != 0:
        raise Exception("test failed:", "unit")
else:
    if os.system('../unit') != 0:
        raise Exception("test failed:", "unit")
if os.system('python ./eval.py') != 0:
    raise Exception("test failed:", "eval")
if os.system('python ./mate_in_n.py') != 0:
    raise Exception("test failed:", "mate in one")
if os.system('python ./king_safety.py') != 0:
    raise Exception("test failed:", "king safety")
