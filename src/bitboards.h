//   GreKo chess engine
//   (c) 2002-2018 Vladimir Medvedev <vrm@bk.ru>
//   http://greko.su

#ifndef BITBOARDS_H
#define BITBOARDS_H

#include "types.h"

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
////////////////////////////////////////////////////////////////////////////////

inline FLD PopLSB(U64& b)
{
	FLD f = LSB(b);
	b ^= BB_SINGLE[f];
	return f;
}
////////////////////////////////////////////////////////////////////////////////

U64  Attacks(FLD f, U64 occ, PIECE piece);
U64  BishopAttacks(FLD f, U64 occ);
U64  BishopAttacksTrace(FLD f, U64 occ);
int  CountBits(U64 b);
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

#endif
