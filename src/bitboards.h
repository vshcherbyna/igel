/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2002-2018 Vladimir Medvedev <vrm@bk.ru> (GreKo author)
*  Copyright (C) 2018-2019 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

#ifndef BITBOARDS_H
#define BITBOARDS_H

#include "types.h"

#if _WIN32 || _WIN64
#include <intrin.h>
#endif

extern U64 BB_SINGLE[64];
extern U64 BB_DIR[64][8];
extern U64 BB_BETWEEN[64][64];

extern U64 BB_PAWN_ATTACKS[64][2];
extern U64 BB_KNIGHT_ATTACKS[64];
extern U64 BB_BISHOP_ATTACKS[64];
extern U64 BB_ROOK_ATTACKS[64];
extern U64 BB_QUEEN_ATTACKS[64];
extern U64 BB_KING_ATTACKS[64];

extern U64 BB_HORIZONTAL[8];
extern U64 BB_VERTICAL[8];

extern U64 BB_FIRST_HORIZONTAL[2];
extern U64 BB_SECOND_HORIZONTAL[2];
extern U64 BB_THIRD_HORIZONTAL[2];
extern U64 BB_FOURTH_HORIZONTAL[2];
extern U64 BB_FIFTH_HORIZONTAL[2];
extern U64 BB_SIXTH_HORIZONTAL[2];
extern U64 BB_SEVENTH_HORIZONTAL[2];
extern U64 BB_EIGHTH_HORIZONTAL[2];

extern U64 BB_PASSED_PAWN_MASK_SIDE[64][2];
extern U64 BB_PASSED_PAWN_MASK_OPP[64][2];
extern U64 BB_DOUBLED_PAWN_MASK[64][2];
extern U64 BB_ISOLATED_PAWN_MASK[64];
extern U64 BB_STRONG_FIELD_MASK[64][2];

extern U64 BB_PAWN_SQUARE[64][2];
extern U64 BB_PAWN_CONNECTED[64];

#if defined (_BTYPE)
inline FLD LSB(U64 b)
{
    assert(b != 0);

#if _WIN32 || _WIN64
    unsigned long ret;
    _BitScanForward64(&ret, b);
    return static_cast<FLD>(ret ^ 63);
#else
    return static_cast<FLD>(__builtin_ctzl(b) ^ 63);
#endif
}
#else
inline FLD LSB(U64 b)
{
    assert(b != 0);

    static const U32 mult = 0x78291acf;
    static const FLD table[64] =
    {
         0, 33, 60, 31,  4, 49, 52, 30,
         3, 39, 13, 54,  8, 44, 42, 29,
         2, 34, 61, 10, 12, 40, 22, 45,
         7, 35, 62, 20, 17, 36, 63, 28,
         1, 32,  5, 59, 58, 14,  9, 57,
        48, 11, 51, 23, 56, 21, 18, 47,
        38,  6, 15, 50, 53, 24, 55, 19,
        43, 16, 25, 41, 46, 26, 27, 37
    };

    U64 x = b ^ (b - 1);
    U32 y = U32(x) ^ U32(x >> 32);
    int index = (y * mult) >> (32 - 6);
    return table[index];
}
#endif

inline FLD PopLSB(U64& b)
{
    FLD f = LSB(b);
    b &= b - 1;
    return f;
}

#if defined (_BTYPE)
inline unsigned int countBits(U64 b)
{
#if _WIN32 || _WIN64
    return __popcnt64(b);
#else
    return __builtin_popcountl(b);
#endif
}
#else
inline unsigned int countBits(U64 b)
{
    if (b == 0)
        return 0;

    static const U64 mask_1 = LL(0x5555555555555555);   // 0101 0101 0101 0101 0101 0101 0101 0101 ...
    static const U64 mask_2 = LL(0x3333333333333333);   // 0011 0011 0011 0011 0011 0011 0011 0011 ...
    static const U64 mask_4 = LL(0x0f0f0f0f0f0f0f0f);   // 0000 1111 0000 1111 0000 1111 0000 1111 ...
    static const U64 mask_8 = LL(0x00ff00ff00ff00ff);   // 0000 0000 1111 1111 0000 0000 1111 1111 ...

    U64 x = (b & mask_1) + ((b >> 1) & mask_1);
    x = (x & mask_2) + ((x >> 2) & mask_2);
    x = (x & mask_4) + ((x >> 4) & mask_4);
    x = (x & mask_8) + ((x >> 8) & mask_8);

    U32 y = U32(x) + U32(x >> 32);
    return (y + (y >> 16)) & 0x3f;
}
#endif

U64  Attacks(FLD f, U64 occ, PIECE piece);
U64  BishopAttacks(FLD f, U64 occ);
U64  BishopAttacksTrace(FLD f, U64 occ);
int  Delta(int dir);
U64  EnumBits(U64 b, int n);
void FindMagicLSB();
void FindMaskB();
void FindMaskR();
void FindMultB();
void FindMultR();
void FindShiftB();
void FindShiftR();
void InitBitboards();
void Print(U64 b);
void PrintArray(const U64* arr);
void PrintHex(U64 b);
U64  QueenAttacks(FLD f, U64 occ);
U64  QueenAttacksTrace(FLD f, U64 occ);
U64  RookAttacks(FLD f, U64 occ);
U64  RookAttacksTrace(FLD f, U64 occ);
U64  Shift(U64 b, int dir);
void TestMagic();

inline U64 Up(U64 b)    { return b << 8; }
inline U64 Down(U64 b)  { return b >> 8; }
inline U64 Left(U64 b)  { return (b & LL(0x7f7f7f7f7f7f7f7f)) << 1; }
inline U64 Right(U64 b) { return (b & LL(0xfefefefefefefefe)) >> 1; }

inline U64 UpLeft(U64 b)    { return (b & LL(0x007f7f7f7f7f7f7f)) << 9; }
inline U64 UpRight(U64 b)   { return (b & LL(0x00fefefefefefefe)) << 7; }
inline U64 DownLeft(U64 b)  { return (b & LL(0x7f7f7f7f7f7f7f00)) >> 7; }
inline U64 DownRight(U64 b) { return (b & LL(0xfefefefefefefe00)) >> 9; }

inline U64 Backward(U64 b, COLOR side) { return (side == WHITE)? (b >> 8) : (b << 8); }
inline U64 DoubleBackward(U64 b, COLOR side) { return (side == WHITE)? (b >> 16) : (b << 16); }
inline U64 BackwardLeft(U64 b, COLOR side) { return (side == WHITE)? DownLeft(b) : UpRight(b); }
inline U64 BackwardRight(U64 b, COLOR side) { return (side == WHITE)? DownRight(b) : UpLeft(b); }

const U64 BB_WHITE_FIELDS = LL(0xaa55aa55aa55aa55);
const U64 BB_BLACK_FIELDS = LL(0x55aa55aa55aa55aa);

const U64 BB_CENTER[2]  = { LL(0x0000181818000000), LL(0x0000001818180000) };
const U64 L1MASK        = LL(0xfefefefefefefefe);
const U64 L2MASK        = LL(0xfcfcfcfcfcfcfcfc);
const U64 R1MASK        = LL(0x7f7f7f7f7f7f7f7f);
const U64 R2MASK        = LL(0x3f3f3f3f3f3f3f3f);

#endif
