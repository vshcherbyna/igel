/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2002-2018 Vladimir Medvedev <vrm@bk.ru> (GreKo author)
*  Copyright (C) 2018-2020 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

#ifndef TYPES_H
#define TYPES_H

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifndef _MSC_VER
#include <stdint.h>
typedef uint8_t U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;
typedef int8_t I8;
typedef int16_t I16;
typedef int32_t I32;
typedef int64_t I64;
#define LL(x) x##LL
#else
typedef unsigned __int8 U8;
typedef unsigned __int16 U16;
typedef unsigned __int32 U32;
typedef unsigned __int64 U64;
typedef __int8 I8;
typedef __int16 I16;
typedef __int32 I32;
typedef __int64 I64;
#define LL(x) x##L
#endif

typedef U8 PIECE;
typedef U8 COLOR;
typedef U8 FLD;

typedef U8 Color;
typedef U8 Square;

typedef I32 EVAL;
typedef I64 NODES;

enum
{
    NOPIECE = 0,
    PW = 2,
    PB = 3,
    NW = 4,
    NB = 5,
    BW = 6,
    BB = 7,
    RW = 8,
    RB = 9,
    QW = 10,
    QB = 11,
    KW = 12,
    KB = 13
};

enum
{
    PAWN   = 2,
    KNIGHT = 4,
    BISHOP = 6,
    ROOK   = 8,
    QUEEN  = 10,
    KING   = 12
};

enum
{
    WHITE  = 0,
    BLACK  = 1,
    COLORS = 2
};

enum
{
    A8, B8, C8, D8, E8, F8, G8, H8,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A1, B1, C1, D1, E1, F1, G1, H1,
    NF
};

inline int Col(FLD f) { return f % 8; }
inline int Row(FLD f) { return f / 8; }

inline COLOR GetColor(PIECE p) { return p & 1; }
inline PIECE GetPieceType(PIECE p) { return p & 0xfe; }

enum
{
    DIR_R  = 0,
    DIR_UR = 1,
    DIR_U  = 2,
    DIR_UL = 3,
    DIR_L  = 4,
    DIR_DL = 5,
    DIR_D  = 6,
    DIR_DR = 7
};

const EVAL CHECKMATE_SCORE  = 32767;
const EVAL TBBASE_SCORE     = 22767;
const EVAL DRAW_SCORE       = 0;

struct Pair
{
    Pair() : mid(0), end(0) {}
    Pair(int m, int e) : mid(m), end(e) {}
    Pair(const Pair& other) : mid(other.mid), end(other.end) {}

    void operator=(const Pair& other) { mid = other.mid; end = other.end; }
    void operator=(int x) { mid = x; end = x; }

    void operator+=(const Pair& other) { mid += other.mid; end += other.end; }
    void operator+=(int x) { mid += x; end += x; }
    
    void operator-=(const Pair& other) { mid -= other.mid; end -= other.end; }
    
    Pair operator-() const { return Pair(-mid, -end); }

    int mid;
    int end;
};

inline Pair operator*(int x, const Pair& p) { return Pair(x * p.mid, x * p.end); }

#define SQUARE_ZERO 0
#define SQUARE_NB   64

// unique number for each piece type on each square
enum PieceSquare : uint32_t {
    PS_NONE = 0,
    PS_W_PAWN = 1,
    PS_B_PAWN = 1 * SQUARE_NB + 1,
    PS_W_KNIGHT = 2 * SQUARE_NB + 1,
    PS_B_KNIGHT = 3 * SQUARE_NB + 1,
    PS_W_BISHOP = 4 * SQUARE_NB + 1,
    PS_B_BISHOP = 5 * SQUARE_NB + 1,
    PS_W_ROOK = 6 * SQUARE_NB + 1,
    PS_B_ROOK = 7 * SQUARE_NB + 1,
    PS_W_QUEEN = 8 * SQUARE_NB + 1,
    PS_B_QUEEN = 9 * SQUARE_NB + 1,
    PS_W_KING = 10 * SQUARE_NB + 1,
    PS_END = PS_W_KING, // pieces without kings (pawns included)
    PS_B_KING = 11 * SQUARE_NB + 1,
    PS_END2 = 12 * SQUARE_NB + 1
};

// An ID used to track the pieces. Max. 32 pieces on board.
enum PieceId {
    PIECE_ID_ZERO = 0,
    PIECE_ID_KING = 30,
    PIECE_ID_WKING = 30,
    PIECE_ID_BKING = 31,
    PIECE_ID_NONE = 32
};

inline PieceId operator++(PieceId& d, int) {

    PieceId x = d;
    d = PieceId(int(d) + 1);
    return x;
}

enum Value : int {
    VALUE_ZERO = 0,
    VALUE_DRAW = 0,
    VALUE_KNOWN_WIN = 10000,
    VALUE_MATE = 32000,
    VALUE_INFINITE = 32001,
    VALUE_NONE = 32002,

    VALUE_TB_WIN_IN_MAX_PLY = VALUE_MATE - 2 * 128,
    VALUE_TB_LOSS_IN_MAX_PLY = -VALUE_TB_WIN_IN_MAX_PLY,
    VALUE_MATE_IN_MAX_PLY = VALUE_MATE - 128,
    VALUE_MATED_IN_MAX_PLY = -VALUE_MATE_IN_MAX_PLY,

    PawnValueMg = 124, PawnValueEg = 206,
    KnightValueMg = 781, KnightValueEg = 854,
    BishopValueMg = 825, BishopValueEg = 915,
    RookValueMg = 1276, RookValueEg = 1380,
    QueenValueMg = 2538, QueenValueEg = 2682,
    Tempo = 28,

    MidgameLimit = 15258, EndgameLimit = 3915
};

#define COLOR_NB 2

struct ExtPieceSquare {
    PieceSquare from[COLOR_NB];
};

enum Piece {
    NO_PIECE,
    W_PAWN = 1, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
    B_PAWN = 9, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
    PIECE_NB = 16
};

// For differential evaluation of pieces that changed since last turn
struct DirtyPiece {

    // Number of changed pieces
    int dirty_num;

    // The ids of changed pieces, max. 2 pieces can change in one move
    PieceId pieceId[2];

    // What changed from the piece with that piece number
    ExtPieceSquare old_piece[2];
    ExtPieceSquare new_piece[2];
};

// Array for finding the PieceSquare corresponding to the piece on the board
extern ExtPieceSquare kpp_board_index[PIECE_NB];

constexpr bool is_ok(PieceId pid) {
    return pid < PIECE_ID_NONE;
}

// Structure holding which tracked piece (PieceId) is where (PieceSquare)
class EvalList {

public:
    // Max. number of pieces without kings is 30 but must be a multiple of 4 in AVX2
    static const int MAX_LENGTH = 32;

    // Array that holds the piece id for the pieces on the board
    PieceId piece_id_list[SQUARE_NB];

    // List of pieces, separate from White and Black POV
    PieceSquare* piece_list_fw() const { return const_cast<PieceSquare*>(pieceListFw); }
    PieceSquare* piece_list_fb() const { return const_cast<PieceSquare*>(pieceListFb); }

    constexpr Square rotate180(Square sq) {
        return (Square)(sq ^ 0x3F);
    }

    // Place the piece pc with piece_id on the square sq on the board
    void put_piece(PieceId piece_id, Square sq, Piece pc)
    {
        const FLD FLIP[2][64] =
        {
            {
                A8, B8, C8, D8, E8, F8, G8, H8,
                A7, B7, C7, D7, E7, F7, G7, H7,
                A6, B6, C6, D6, E6, F6, G6, H6,
                A5, B5, C5, D5, E5, F5, G5, H5,
                A4, B4, C4, D4, E4, F4, G4, H4,
                A3, B3, C3, D3, E3, F3, G3, H3,
                A2, B2, C2, D2, E2, F2, G2, H2,
                A1, B1, C1, D1, E1, F1, G1, H1
            },

            {
                A1, B1, C1, D1, E1, F1, G1, H1,
                A2, B2, C2, D2, E2, F2, G2, H2,
                A3, B3, C3, D3, E3, F3, G3, H3,
                A4, B4, C4, D4, E4, F4, G4, H4,
                A5, B5, C5, D5, E5, F5, G5, H5,
                A6, B6, C6, D6, E6, F6, G6, H6,
                A7, B7, C7, D7, E7, F7, G7, H7,
                A8, B8, C8, D8, E8, F8, G8, H8
            }
        };

        sq = FLIP[BLACK][sq];
        Square rev = rotate180(sq);

        assert(is_ok(piece_id));
        if (pc != NO_PIECE)
        {
            pieceListFw[piece_id] = PieceSquare(kpp_board_index[pc].from[WHITE] + sq);
            pieceListFb[piece_id] = PieceSquare(kpp_board_index[pc].from[BLACK] + rev);
            piece_id_list[sq] = piece_id;
        }
        else
        {
            pieceListFw[piece_id] = PS_NONE;
            pieceListFb[piece_id] = PS_NONE;
            piece_id_list[sq]     = piece_id;
        }
    }

    // Convert the specified piece_id piece to ExtPieceSquare type and return it
    ExtPieceSquare piece_with_id(PieceId piece_id) const
    {
        ExtPieceSquare eps;
        eps.from[WHITE] = pieceListFw[piece_id];
        eps.from[BLACK] = pieceListFb[piece_id];
        return eps;
    }

public:
    PieceSquare pieceListFw[MAX_LENGTH];
    PieceSquare pieceListFb[MAX_LENGTH];
};
#endif
