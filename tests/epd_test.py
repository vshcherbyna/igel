#
#  Igel - a UCI chess playing engine derived from GreKo 2018.01
#
#  Copyright (C) 2015 Niklas Fiekas
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
#  

#!/usr/bin/python3

#
#   Based on work from https://gist.github.com/niklasf/73c9565719d124af64ff
#

import chess
import chess.engine
import time
import sys

def test_epd(engine, epd,  wait_time):
    position = chess.Board()
    epd_info = position.set_epd(epd)
    info = engine.analyse(position, chess.engine.Limit(time=wait_time))
    bestmove = info["pv"]
    
    if bestmove[0] in epd_info["bm"]:
        print ("%s (expecting %s): +1" % (
            epd_info["id"],
            " or ".join(position.san(bm) for bm in epd_info["bm"])))
        return 1.0
    else:
        print ("%s (expecting %s): +0 (got %s)" % (
            epd_info["id"],
            " or ".join(position.san(bm) for bm in epd_info["bm"]),
            position.san(bestmove[0])))
        return 0.0

def run_test(eng,  wt,  f, mem, threads):
    engine = chess.engine.SimpleEngine.popen_uci("../igel")
    engine.configure({"Threads": threads})
    engine.configure({"Hash": mem})

    file = open(f, 'r') 
    epds = file.read()

    score = 0.0
    total = 0
    
    for epd in epds.split("\n"):
        sys.stdout.flush()
        if len(epd):
            total = total + 1
            score += test_epd(engine, epd,  wt)
        sys.stdout.flush()

    engine.quit()    
    print ("%g / %g" % (score,  total))
    sys.stdout.flush()
    return score
