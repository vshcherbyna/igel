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
import epd_test

if epd_test.run_test('../igel',  10,  'epds/null_move.epd') != 4:
    raise Exception("test failed:", "null move test")

if epd_test.run_test('../igel',  10,  'epds/bk.epd') != 16:
    raise Exception("test failed:", "bratko kopec test")

if epd_test.run_test('../igel',  10,  'epds/dann_corbit_tune.epd') != 114:
    raise Exception("test failed:", "dann corbit tune test")

if os.system('python ./go_infinite_test.py') != 0:
    raise Exception("test failed:", "go infinite test")

if os.system('python ./ci.py') != 0:
    raise Exception("test failed:", "ci")
