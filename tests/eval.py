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

import operator
import subprocess
import re
import chess
import chess.uci
    
def obtain_eval(fen):
    cmd = subprocess.Popen(['../src/igel'], stdout=subprocess.PIPE, stdin=subprocess.PIPE, stderr=subprocess.PIPE)
    if fen == None:
        cmd_line = 'eval\n'
    else:
        cmd_line = 'position fen ' + fen + '\nboard\neval\n'
    stdout_data = cmd.communicate(input=cmd_line)[0]
    digits = re.findall(r'[+-]?\d+(?:\.\d+)?', stdout_data)
    ret = int(digits[1])

    board = chess.Board(fen)
    
    if fen != None:
        print(fen)
    print(board)
    print("eval: %s" % ret)
    #print(ret)
    return ret

def eval_assert(expected_eval,  op,  engine_eval):
    if op(expected_eval,  engine_eval) != True:
        raise Exception(expected_eval, op,  engine_eval)
            
def eval_default_board():
    eval_assert(0,  operator.eq,  obtain_eval(None))
    eval_assert(0,  operator.eq,  obtain_eval('rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1'))
    
def eval_pawn_structure():
    
    #
    #   symmetric pawn structure
    #
    
    eval_assert(0,  operator.eq,  obtain_eval('rnbqkbnr/ppp2ppp/4p3/3p4/3P4/4P3/PPP2PPP/RNBQKBNR w KQkq - 0 1'))
    
    #
    #   double pawn structure
    #
    
    eval_assert(0,  operator.gt,  obtain_eval('rnbqkbnr/ppp2ppp/4p3/3p4/3P4/3P4/PPP2PPP/RNBQKBNR w KQkq - 0 1'))
    eval_assert(0,  operator.lt,  obtain_eval('rnbqkbnr/ppp2ppp/4p3/3p4/3P4/3P4/PPP2PPP/RNBQKBNR b KQkq - 0 1'))
    
def eval_knight_position():
    
    #
    #   central knight
    #
    
    eval_assert(0,  operator.lt,  obtain_eval('rnbqkbnr/ppp1pppp/3p4/8/2N5/3P4/PPP1PPPP/R1BQKBNR w KQkq - 0 1'))
    
def main():
    eval_default_board()
    eval_pawn_structure()
    eval_knight_position()
    
if __name__ == "__main__":
    main()
