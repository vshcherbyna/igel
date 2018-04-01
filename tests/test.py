#
#  Igel - a UCI chess playing engine derived from GreKo 2018.01
#
#  Copyright (C) 2018 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

#!/usr/bin/python

import os

if os.system('python ./bratko_kopec_test.py') != 0:
    raise Exception("test failed:", "bratko kopec test")
if os.system('python ./null_move_test.py') != 0:
    raise Exception("test failed:", "null move test")
if os.system('python ./null_move_test.py') != 0:
    raise Exception("test failed:", "dann corbit test")
if os.system('python ./dann_corbit_test.py') != 0:
    raise Exception("test failed:", "go infinite test")
if os.system('python ./ci.py') != 0:
    raise Exception("test failed:", "ci")
