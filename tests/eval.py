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

import operator
import subprocess
import re
import chess
import chess.uci
    
def obtain_eval(fen):
    cmd = subprocess.Popen(['../igel'], stdout=subprocess.PIPE, stdin=subprocess.PIPE, stderr=subprocess.PIPE)
    if fen == None:
        cmd_line = 'eval\n'
    else:
        cmd_line = 'position fen ' + fen + '\neval\n'
    stdout_data = cmd.communicate(input=cmd_line.encode())[0]
    digits = re.findall(r'[+-]?\d+(?:\.\d+)?', stdout_data.decode('utf-8'))
    ret = int(digits[1])        
    if fen != None:
        print(fen)
    board = chess.Board(fen)
    print(board)
    print("eval: " + str(ret))
    return ret

def eval_assert(expected_eval,  op,  engine_eval):
    if op(expected_eval,  engine_eval) != True:
        raise Exception(expected_eval, op,  engine_eval)
            
def eval_default_board():
    
    #
    #   evaluation of standard position should be 0
    #
    
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
    
    eval_assert(0,  (lambda a, b: True if a > b and b > -30 else False),  obtain_eval('rnbqkbnr/ppp2ppp/4p3/3p4/3P4/3P4/PPP2PPP/RNBQKBNR w KQkq - 0 1'))
    eval_assert(0,  (lambda a, b: True if a < b and b <= 30 else False),  obtain_eval('rnbqkbnr/ppp2ppp/4p3/3p4/3P4/3P4/PPP2PPP/RNBQKBNR b KQkq - 0 1'))
    
def eval_knight_position():
    
    #
    #   symmetric knight structure
    #
    
    eval_assert(0,  operator.eq,  obtain_eval('rnbqkb1r/ppp2ppp/4pn2/3p4/3P4/2N1P3/PPP2PPP/R1BQKBNR w KQkq - 0 1'))
    eval_assert(0,  operator.eq,  obtain_eval('r1bqkb1r/ppp2ppp/2n1pn2/3p4/3P4/2N1PN2/PPP2PPP/R1BQKB1R w KQkq - 0 1'))

    #
    #   central knight
    #
    
    eval_assert(0,  (lambda a, b: True if a < b and b <= 45 else False),  obtain_eval('rnbqkbnr/ppp1pppp/3p4/8/2N5/3P4/PPP1PPPP/R1BQKBNR w KQkq - 0 1'))
    eval_assert(0,  operator.eq,  obtain_eval('r1bqkb1r/ppp2ppp/2n1p3/3pN3/3Pn3/2N1P3/PPP2PPP/R1BQKB1R w KQkq - 0 1'))
    eval_assert(0,  (lambda a, b: True if a > b and b > -30 else False),  obtain_eval('r1bqkb1r/ppp2ppp/2n1p3/3p4/3Pn3/2N1PN2/PPP2PPP/R1BQKB1R w KQkq - 0 1'))
    
    #
    #   knight on the rim is dim
    #

    eval_assert(0,  (lambda a, b: True if a < b and b <= 5 else False),  obtain_eval('r1bqkb1r/ppp2ppp/4p3/n2pN3/3Pn3/2N1P3/PPP2PPP/R1BQKB1R w KQkq - 0 1'))
    eval_assert(0,  operator.eq,  obtain_eval('r1bqkb1r/ppp2ppp/4p3/n2pN3/3Pn2N/4P3/PPP2PPP/R1BQKB1R w KQkq - 0 1'))
    eval_assert(0,  (lambda a, b: True if a < b and b <= 15 else False),  obtain_eval('r1bqkb1r/ppp2ppp/4p3/n2pN2N/3Pn3/4P3/PPP2PPP/R1BQKB1R w KQkq - 0 1'))
    eval_assert(0,  operator.eq,  obtain_eval('r1bqkb1r/ppp2ppp/4p3/3pN2N/n2Pn3/4P3/PPP2PPP/R1BQKB1R w KQkq - 0 1'))

def main():
    eval_default_board()
    eval_pawn_structure()
    eval_knight_position()
    
if __name__ == "__main__":
    main()
