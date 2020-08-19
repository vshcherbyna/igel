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

#ifndef POSITION_H
#define POSITION_H

#include "bitboards.h"
#include "evaluate.h"
#include "nnue/nnue_accumulator.h"

#define STD_POSITION "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

enum
{
    KINGSIDE = 0,
    QUEENSIDE = 1
};

// [color][flank]
const U8 CASTLINGS[2][2] =
{
    { 0x01, 0x02 },
    { 0x10, 0x20 }
};

class Move
{
public:
    Move() : m_data(0) {}
    Move(U32 x) : m_data(x) {}

    Move(U32 from, U32 to, U32 piece) :
        m_data(from | (to << 6) | (piece << 20)) {}
    Move(U32 from, U32 to, U32 piece, U32 captured) :
        m_data(from | (to << 6) | (piece << 20) | (captured << 16)) {}
    Move(U32 from, U32 to, U32 piece, U32 captured, U32 promotion) :
        m_data(from | (to << 6) | (piece << 20) | (captured << 16) | (promotion << 12)) {}

    FLD From() const { return m_data & 0x3f; }
    FLD To() const { return (m_data >> 6) & 0x3f; }
    FLD Piece() const { return (m_data >> 20) & 0x0f; }
    FLD Captured() const { return (m_data >> 16) & 0x0f; }
    FLD Promotion() const { return (m_data >> 12) & 0x0f; }

    operator U32() const { return m_data; }

private:
    U32 m_data;
};

class Position;

struct PawnHashEntry
{
    PawnHashEntry() : m_pawnHash(0) {}
    void Read(const Position& pos);

    U32 m_pawnHash;
    I8  m_ranks[10][2];

    U64 m_passedPawns[2];
    U64 m_doubledPawns[2];
    U64 m_isolatedPawns[2];
    U64 m_strongFields[2];
    U64 m_backwardPawns;
};

struct Undo
{
    U8   m_castlings;
    FLD  m_ep;
    int  m_fifty;
    U64  m_hash;
    Move m_mv;

#if defined(EVAL_NNUE)
    Eval::NNUE::Accumulator accumulator;
    DirtyPiece dirtyPiece;
    Undo* previous;
#endif
};

class Position
{
public:
    U64    Bits(PIECE p) const { return m_bits[p]; }
    U64    BitsAll(COLOR side) const { return m_bitsAll[side]; }
    U64    BitsAll() const { return m_bitsAll[WHITE] | m_bitsAll[BLACK]; }
    U8     Castlings() const { return m_castlings; }
    bool   CanCastle(COLOR side, U8 flank) const;
    int    Count(PIECE p) const { return m_count[p]; }
    FLD    EP() const { return m_ep; }
    std::string FEN() const;
    int    Fifty() const { return m_fifty; }
    U64    GetAttacks(FLD to, COLOR side, U64 occ) const;
    U64    Hash() const;
    bool   InCheck() const { return IsAttacked(King(m_side), m_side ^ 1); }
    bool   IsAttacked(FLD f, COLOR side) const;
    FLD    King(COLOR side) const { return m_Kings[side]; }
    Move   LastMove() const { return (m_undoSize > 0)? m_undos[m_undoSize - 1].m_mv : Move(); }
    bool   MakeMove(Move mv);
    void   MakeNullMove();
    int    MatIndex(COLOR side) const { return m_matIndex[side]; }
    U32    PawnHash() const { return m_pawnHash; }
    int    Ply() const { return m_ply; }
    void   Print() const;
    int    Repetitions() const;
    Pair   Score() const { return m_score; }
    bool   SetFEN(const std::string& fen);
    void   SetInitial();
    COLOR  Side() const { return m_side; }
    Color  side_to_move() const{ return m_side; }
    void   UnmakeMove();
    void   UnmakeNullMove();
    bool   isInitialPosition();
    const PIECE& operator[] (FLD f) const { return m_board[f]; }
    static void  InitHashNumbers();
    bool NonPawnMaterial();
    EVAL nonPawnMaterial();

#if defined(EVAL_NNUE)
    Undo * state() const { return m_state; }
    const EvalList * eval_list() const;
    inline PieceId piece_id_on(Square sq) const;
    Undo * m_state;
    inline Piece NNUEPieceAdaptor(PIECE p);
#endif

private:
    void Clear();
    void Put(FLD f, PIECE p);
    void Put(FLD f, PIECE p, PieceId & next_piece_id);
    void Remove(FLD f);
    void MovePiece(PIECE p, FLD from, FLD to);
    EVAL nonPawnMaterial(COLOR side);

    static U64 s_hash[64][14];
    static U64 s_hashSide[2];
    static U64 s_hashCastlings[256];
    static U64 s_hashEP[256];
    static U32 s_pawnHash[64][14];

    static const int s_matIndexDelta[14];

    U64   m_bits[14];
    U64   m_bitsAll[2];
    PIECE m_board[64];
    U8    m_castlings;
    int   m_count[14];
    FLD   m_ep;
    int   m_fifty;
    U64   m_hash;
    FLD   m_Kings[2];
    int   m_matIndex[2];
    U32   m_pawnHash;
    int   m_ply;
    Pair  m_score;
    COLOR m_side;

    enum { MAX_UNDO = 1024 };
    Undo m_undos[MAX_UNDO];
    int m_undoSize;
    bool m_initialPosition;
    EvalList evalList;

public:
    static const int m_pawnHashSize = 131072;
    PawnHashEntry m_pawnHashTable[m_pawnHashSize];
};

const FLD AX[2] = { A1, A8 };
const FLD BX[2] = { B1, B8 };
const FLD CX[2] = { C1, C8 };
const FLD DX[2] = { D1, D8 };
const FLD EX[2] = { E1, E8 };
const FLD FX[2] = { F1, F8 };
const FLD GX[2] = { G1, G8 };
const FLD HX[2] = { H1, H8 };

extern const Move MOVE_O_O[2];
extern const Move MOVE_O_O_O[2];

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

#endif
