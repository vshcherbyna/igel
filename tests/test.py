#
#  Igel - a UCI chess playing engine derived from GreKo 2018.01
#
#  Copyright (C) 2018-2020 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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
import sys
import epd_test
import time
import sys

from concurrent.futures import ThreadPoolExecutor
import threading

skip_errors = "false"

if len(sys.argv) > 1:
    skip_errors = sys.argv[1]

lock = threading.Lock()
score = 0
improvements = 0
def_mem = 256
def_threads = 1
def_search_time = 10

def run_epd_test(name,  expected_pass,  epd,  tc, mem, threads):
    global improvements
    global skip_errors
    global score
    print ("running test:" + name)
    passed = int(epd_test.run_test('../igel',  tc,  epd, mem, threads))
    lock.acquire()
    score = score + passed
    lock.release()
    sys.stdout.flush()

    lock.acquire()
    if passed < expected_pass:
        if "skip" != skip_errors:
            raise Exception("test failed:", name + ": required " + str(expected_pass) + " got only " + str(passed))
        else:
            print ("test failed:"  + name + ": required " + str(expected_pass) + " got only " + str(passed))
    elif passed > expected_pass:
        print ("Improvement detected in " + name + " " + str(passed - expected_pass))
        improvements = improvements + 1
    lock.release()

if "skip" != skip_errors:
    if os.system('python ./ci.py') != 0:
        raise Exception("test failed:", "ci")

    if os.system('python ./go_infinite_test.py') != 0:
        raise Exception("test failed:", "go infinite test")

executor = ThreadPoolExecutor(max_workers=1)

#
#   fundamental test suites
#

executor.submit(run_epd_test, 'bratko kopec test',  17,  'epds/bk.epd', 60, def_mem, def_threads)
executor.submit(run_epd_test, 'null move test',  4,  'epds/null_move.epd', 60, def_mem, def_threads)
executor.submit(run_epd_test, 'win at chess test',  296,  'epds/wacnew.epd', def_search_time, def_mem, def_threads)
executor.submit(run_epd_test, 'dann corbit tune test',  115,  'epds/dann_corbit_tune.epd', def_search_time, def_mem, def_threads)

#
#   strategic test suites
#

executor.submit(run_epd_test, 'sts undermine test',  83,  'epds/sts1.epd', def_search_time, def_mem, def_threads)
executor.submit(run_epd_test, 'sts open files and diagonals test',  80,  'epds/sts2.epd', def_search_time, def_mem, def_threads)
executor.submit(run_epd_test, 'sts centralization test',  79,  'epds/sts3.epd', def_search_time, def_mem, def_threads)
executor.submit(run_epd_test, 'sts square vacancy test',  77,  'epds/sts4.epd', def_search_time, def_mem, def_threads)
executor.submit(run_epd_test, 'sts bishop vs knight test',  78,  'epds/sts5.epd', def_search_time, def_mem, def_threads)
executor.submit(run_epd_test, 'sts recapturing test',  70,  'epds/sts6.epd', def_search_time, def_mem, def_threads)
executor.submit(run_epd_test, 'sts offer of simplication test',  60,  'epds/sts7.epd', def_search_time, def_mem, def_threads)
executor.submit(run_epd_test, 'sts advacement of f/h/g pawns test',  59,  'epds/sts8.epd', def_search_time, def_mem, def_threads)
executor.submit(run_epd_test, 'sts advacement of a/b/c pawns test',  67,  'epds/sts9.epd', def_search_time, def_mem, def_threads)
executor.submit(run_epd_test, 'sts simplification test',  77,  'epds/sts10.epd', def_search_time, def_mem, def_threads)
executor.submit(run_epd_test, 'sts king activity test',  66,  'epds/sts11.epd', def_search_time, def_mem, def_threads)
executor.submit(run_epd_test, 'sts central control test',  75,  'epds/sts12.epd', def_search_time, def_mem, def_threads)
executor.submit(run_epd_test, 'sts pawn play in the center test',  71,  'epds/sts13.epd', def_search_time, def_mem, def_threads)
executor.submit(run_epd_test, 'sts rooks/queens on 7th test',  74,  'epds/sts14.epd', def_search_time, def_mem, def_threads)
executor.submit(run_epd_test, 'sts pointless exchange test',  35,  'epds/sts15.epd', def_search_time, def_mem, def_threads)

executor.shutdown(wait=True)

if improvements > 0:
    print ("Improvement detected: " + str(improvements))

print ("Score: " + str(score))
