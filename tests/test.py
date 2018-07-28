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
import sys
import epd_test
import warnings

skip_errors = "false"

if len(sys.argv) > 1:
    skip_errors = sys.argv[1]

improvements = 0

def run_epd_test(name,  expected_pass,  epd,  tc):
    global improvements
    global skip_errors
    print ("running test: %s") % (name)
    passed = int(epd_test.run_test('../igel',  tc,  epd))
    if passed < expected_pass:
        if "skip" != skip_errors:
            raise Exception("test failed:", name + ": required " + str(expected_pass) + " got only " + str(passed))
        else:
            warnings.warn("test failed:"  + name + ": required " + str(expected_pass) + " got only " + str(passed))
    elif passed > expected_pass:
        print ("\033[1;32;40mImprovement detected in %s: %s\x1b[0m") % (name,  passed - expected_pass)
        improvements = improvements + 1

if os.system('python ./ci.py') != 0:
    raise Exception("test failed:", "ci")

if os.system('python ./go_infinite_test.py') != 0:
    raise Exception("test failed:", "go infinite test")

#
#   fundamental test suites
#

run_epd_test('null move test',  4,  'epds/null_move.epd',  10000)
run_epd_test('bratko kopec test',  18,  'epds/bk.epd',  10000)
run_epd_test('win at chess test',  286,  'epds/wacnew.epd',  1000)

#
#   strategic test suites
#

run_epd_test('sts undermine test',  76,  'epds/sts1.epd',  10000)
run_epd_test('sts open files and diagonals test',  64,  'epds/sts2.epd',  10000)
run_epd_test('sts centralization test',  67,  'epds/sts3.epd',  10000)
run_epd_test('sts square vacancy test',  60,  'epds/sts4.epd',  10000)
run_epd_test('sts bishop vs knight test',  70,  'epds/sts5.epd',  10000)
run_epd_test('sts recapturing test',  62,  'epds/sts6.epd',  10000)
run_epd_test('sts offer of simplication test',  54,  'epds/sts7.epd',  10000)
run_epd_test('sts advacement of f/h/g pawns test',  44,  'epds/sts8.epd',  10000)
run_epd_test('sts advacement of a/b/c pawns test',  55,  'epds/sts9.epd',  10000)
run_epd_test('sts simplification test',  73,  'epds/sts10.epd',  10000)
run_epd_test('sts king activity test',  53,  'epds/sts11.epd',  10000)
run_epd_test('sts central control test',  68,  'epds/sts12.epd',  10000)
run_epd_test('sts pawn play in the center test',  63,  'epds/sts13.epd',  10000)
run_epd_test('sts rooks/queens on 7th test',  68,  'epds/sts14.epd',  10000)
run_epd_test('sts pointless exchange test',  35,  'epds/sts15.epd',  10000)        

#
#   one day I can enable this one :)
#

#run_epd_test('dann corbit tune test',  116,  'epds/dann_corbit_tune.epd',  10000)

if improvements > 0:
    print ("\033[1;32;40mImprovement detected: %s\x1b[0m") % (improvements)
