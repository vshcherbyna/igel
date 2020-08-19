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

#include "notation.h"
#include "position.h"
#include "utils.h"
#include "evaluate.h"

#include <memory>

extern Pair pieceSquareTables[14][64];

const Move MOVE_O_O[2]   = { Move(E1, G1, KW), Move(E8, G8, KB) };
const Move MOVE_O_O_O[2] = { Move(E1, C1, KW), Move(E8, C8, KB) };

U64 Position::s_hash[64][14];
U64 Position::s_hashSide[2];
U64 Position::s_hashCastlings[256];
U64 Position::s_hashEP[256];
U32 Position::s_pawnHash[64][14];

const int Position::s_matIndexDelta[14] = { 0, 0, 0, 0, 3, 3, 3, 3, 5, 5, 10, 10, 0, 0 };

bool Position::CanCastle(COLOR side, U8 flank) const
{
    if (InCheck())
        return false;
    if ((m_castlings & CASTLINGS[side][flank]) == 0)
        return false;

    COLOR opp = side ^ 1;

    switch (flank)
    {
    case KINGSIDE:
        return !m_board[FX[side]] && !m_board[GX[side]] && !IsAttacked(FX[side], opp) && !IsAttacked(GX[side], opp);
    case QUEENSIDE:
        return !m_board[DX[side]] && !m_board[CX[side]] && !m_board[BX[side]] && !IsAttacked(DX[side], opp) && !IsAttacked(CX[side], opp);
    default:
        return false;
    }
}

void Position::Clear()
{
#if defined(EVAL_NNUE)
    std::memset(evalList.piece_id_list, 0, sizeof(evalList.piece_id_list));
    std::memset(evalList.pieceListFw, 0, sizeof(evalList.pieceListFw));
    std::memset(evalList.pieceListFb, 0, sizeof(evalList.pieceListFb));
    std::memset(m_undos, 0, sizeof(m_undos));

    m_state = &m_undos[0];
#endif

    for (FLD f = 0; f < 64; ++f)
        m_board[f] = NOPIECE;

    for (PIECE p = 0; p < 14; ++p)
    {
        m_bits[p] = 0;
        m_count[p] = 0;
    }

    m_bitsAll[WHITE] = m_bitsAll[BLACK] = 0;
    m_castlings = 0;
    m_ep = NF;
    m_fifty = 0;
    m_hash = 0;
    m_Kings[WHITE] = m_Kings[BLACK] = NF;
    m_matIndex[WHITE] = m_matIndex[BLACK] = 0;
    m_pawnHash = 1;
    m_ply = 0;
    m_score = Pair();
    m_side = WHITE;
    m_undoSize = 0;
}

std::string Position::FEN() const
{
    static const std::string names = "-?PpNnBbRrQqKk";
    std::stringstream fen;
    int empty = 0;

    for (FLD f = 0; f < 64; ++f)
    {
        PIECE p = m_board[f];
        if (p)
        {
            if (empty > 0)
            {
                fen << empty;
                empty = 0;
            }
            fen << names.substr(p, 1);
        }
        else
            ++empty;
        if (Col(f) == 7)
        {
            if (empty > 0)
                fen << empty;
            if (f < 63)
                fen << "/";
            empty = 0;
        }
    }
    if (empty > 0)
        fen << empty;

    if (m_side == WHITE)
        fen << " w ";
    else
        fen << " b ";

    if (m_castlings & 0xff)
    {
        if (m_castlings & CASTLINGS[WHITE][KINGSIDE])
            fen << "K";
        if (m_castlings & CASTLINGS[WHITE][QUEENSIDE])
            fen << "Q";
        if (m_castlings & CASTLINGS[BLACK][KINGSIDE])
            fen << "k";
        if (m_castlings & CASTLINGS[BLACK][QUEENSIDE])
            fen << "q";
    }
    else
        fen << "-";
    fen << " ";

    if (m_ep == NF)
        fen << "-";
    else
        fen << FldToStr(m_ep);
    fen << " ";

    fen << m_fifty << " " << m_ply / 2 + 1;
    return fen.str();
}

U64 Position::GetAttacks(FLD to, COLOR side, U64 occ) const
{
    U64 att = 0;

    att |= BB_PAWN_ATTACKS[to][side ^ 1] & Bits(PW | side);
    att |= BB_KNIGHT_ATTACKS[to] & Bits(NW | side);
    att |= BB_KING_ATTACKS[to] & Bits(KW | side);
    att |= BishopAttacks(to, occ) & (Bits(BW | side) | Bits(QW | side));
    att |= RookAttacks(to, occ) & (Bits(RW | side) | Bits(QW | side));
    return att;
}

U64 Position::Hash() const
{
    return m_hash ^ s_hashSide[m_side] ^ s_hashCastlings[m_castlings] ^ s_hashEP[m_ep];
}

void Position::InitHashNumbers()
{
    RandSeed(30147);

    for (FLD f = 0; f < 64; ++f)
    {
        for (PIECE p = 0; p < 14; ++p)
        {
            s_hash[f][p] = Rand64();
            s_pawnHash[f][p] = (p == PW || p == PB)? Rand32() : 0;
        }
    }

    s_hashSide[WHITE] = Rand64();
    s_hashSide[BLACK] = Rand64();

    for (int i = 0; i < 256; ++i)
    {
        s_hashCastlings[i] = Rand64();
        s_hashEP[i] = Rand64();
    }
}

bool Position::IsAttacked(FLD f, COLOR side) const
{
    if (BB_PAWN_ATTACKS[f][side ^ 1] & Bits(PAWN | side))
        return true;
    if (BB_KNIGHT_ATTACKS[f] & Bits(KNIGHT | side))
        return true;
    if (BB_KING_ATTACKS[f] & Bits(KING | side))
        return true;

    U64 x, occ = BitsAll();

    x = BB_BISHOP_ATTACKS[f] & (Bits(BISHOP | side) | Bits(QUEEN | side));
    while (x)
    {
        FLD from = PopLSB(x);
        if ((BB_BETWEEN[from][f] & occ) == 0)
            return true;
    }

    x = BB_ROOK_ATTACKS[f] & (Bits(ROOK | side) | Bits(QUEEN | side));
    while (x)
    {
        FLD from = PopLSB(x);
        if ((BB_BETWEEN[from][f] & occ) == 0)
            return true;
    }

    return false;
}

#if defined(EVAL_NNUE)
inline PieceId Position::piece_id_on(Square sq) const
{
    Square flipped = FLIP[BLACK][sq];
    PieceId pid = evalList.piece_id_list[flipped];
    assert(is_ok(pid));

    return pid;
}
#endif

bool Position::MakeMove(Move mv)
{
    assert(m_undoSize < MAX_UNDO);
    Undo & undo = m_undos[m_undoSize++];

    undo.m_castlings = m_castlings;
    undo.m_ep = m_ep;
    undo.m_fifty = m_fifty;
    undo.m_hash = Hash();
    undo.m_mv = mv;

#if defined(EVAL_NNUE)
    undo.previous = m_undoSize == 1 ? nullptr : m_state;
    m_state = &undo;

    m_state->accumulator.computed_accumulation = false;
    m_state->accumulator.computed_score = false;
    PieceId dp0 = PIECE_ID_NONE;
    PieceId dp1 = PIECE_ID_NONE;
    auto & dp = m_state->dirtyPiece;
    dp.dirty_num = 1;
#endif

    FLD from = mv.From();
    FLD to = mv.To();
    PIECE piece = mv.Piece();
    PIECE captured = mv.Captured();
    PIECE promotion = mv.Promotion();

    //assert(!(captured && promotion));
    assert((piece >= 2) && (piece <= 13));

    COLOR side = m_side;
    COLOR opp = side ^ 1;

    if (m_initialPosition)
        m_initialPosition = false;

    ++m_fifty;
    if (captured) {
#if defined(EVAL_NNUE)
        auto nnueTo = to == m_ep ? to + 8 - 16 * side : to;
#endif
        m_fifty = 0;
        if (to == m_ep)
            Remove(to + 8 - 16 * side);
        else
            Remove(to);

#if defined(EVAL_NNUE)
        dp.dirty_num = 2; // 2 pieces moved
        dp1 = piece_id_on(nnueTo);
        dp.pieceId[1] = dp1;
        dp.old_piece[1] = evalList.piece_with_id(dp1);
        evalList.put_piece(dp1, to, NO_PIECE);
        dp.new_piece[1] = evalList.piece_with_id(dp1);
#endif
    }

#if defined(EVAL_NNUE)
    auto castling = (mv == MOVE_O_O[side]) || (mv == MOVE_O_O_O[side]);

    // handle non-castling moves
    if (!castling) {
        dp0 = piece_id_on(from);
        dp.pieceId[0] = dp0;
        dp.old_piece[0] = evalList.piece_with_id(dp0);
        evalList.put_piece(dp0, to, NNUEPieceAdaptor(piece));
        dp.new_piece[0] = evalList.piece_with_id(dp0);
    }
#endif

    MovePiece(piece, from, to);

    m_ep = NF;

    switch (piece)
    {
        case PW:
        case PB:
            m_fifty = 0;
            if (to - from == -16 + 32 * side)
                m_ep = from - 8 + 16 * side;
            else if (promotion)
            {
                Remove(to);
                Put(to, promotion);
#if defined(EVAL_NNUE)
                dp0 = piece_id_on(to);
                evalList.put_piece(dp0, to, NNUEPieceAdaptor(promotion));
                dp.new_piece[0] = evalList.piece_with_id(dp0);
#endif
            }
            break;
        case KW:
        case KB:
            m_Kings[side] = to;
#if defined(EVAL_NNUE)
            FLD rfrom, rto;
#endif
            if (mv == MOVE_O_O[side]) {
#if defined(EVAL_NNUE)
                assert(castling);
                rfrom = HX[side];
                rto = FX[side];
#endif
                Remove(HX[side]);
                Put(FX[side], ROOK | side);
            }
            else if (mv == MOVE_O_O_O[side]) {
#if defined(EVAL_NNUE)
                assert(castling);
                rfrom = AX[side];
                rto = DX[side];
#endif
                Remove(AX[side]);
                Put(DX[side], ROOK | side);
            }
#if defined(EVAL_NNUE)
            if (castling) {
                dp.dirty_num = 2; // 2 pieces moved
                dp0 = piece_id_on(from);
                dp1 = piece_id_on(rfrom);
                dp.pieceId[0] = dp0;
                dp.old_piece[0] = evalList.piece_with_id(dp0);
                evalList.put_piece(dp0, to, side == WHITE ? W_KING : B_KING);
                dp.new_piece[0] = evalList.piece_with_id(dp0);
                dp.pieceId[1] = dp1;
                dp.old_piece[1] = evalList.piece_with_id(dp1);
                evalList.put_piece(dp1, rto, side == WHITE ? W_ROOK : B_ROOK);
                dp.new_piece[1] = evalList.piece_with_id(dp1);
            }
#endif
            break;
        default:
            break;
    }

    static const U8 castlingsDelta[64] =
    {
        0xdf, 0xff, 0xff, 0xff, 0xcf, 0xff, 0xff, 0xef,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xfd, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xfe
    };

    m_castlings &= castlingsDelta[from];
    m_castlings &= castlingsDelta[to];

    ++m_ply;
    m_side ^= 1;

    if (IsAttacked(King(side), opp))
    {
        UnmakeMove();
        return false;
    }

    return true;
}

void Position::UnmakeMove()
{
    assert(m_undoSize);

    if (!m_undoSize)
        return;

    Undo& undo = m_undos[--m_undoSize];
    Move mv = undo.m_mv;
    FLD from = mv.From();
    FLD to = mv.To();
    PIECE piece = mv.Piece();
    PIECE captured = mv.Captured();

    m_castlings = undo.m_castlings;
    m_ep = undo.m_ep;
    m_fifty = undo.m_fifty;

    COLOR opp = m_side;
    COLOR side = opp ^ 1;

#if defined(EVAL_NNUE)
    auto castling = (mv == MOVE_O_O[side]) || (mv == MOVE_O_O_O[side]);

    if (!castling) {
        PieceId dp0 = m_state->dirtyPiece.pieceId[0];
        evalList.put_piece(dp0, from, NNUEPieceAdaptor(piece));
    }
#endif

    Remove(to);

    if (captured) {
#if defined(EVAL_NNUE)
        assert(castling == false);
        auto nnueTo = to == m_ep ? to + 8 - 16 * side : to;
#endif
        if (to == m_ep)
            Put(to + 8 - 16 * side, captured);
        else
            Put(to, captured);
#if defined(EVAL_NNUE)
        PieceId dp1 = m_state->dirtyPiece.pieceId[1];
        assert(evalList.piece_with_id(dp1).from[WHITE] == PS_NONE);
        assert(evalList.piece_with_id(dp1).from[BLACK] == PS_NONE);
        evalList.put_piece(dp1, nnueTo, NNUEPieceAdaptor(captured));
#endif
    }

    Put(from, piece);

    switch (piece)
    {
    case KW:
    case KB:
        m_Kings[side] = from;
#if defined(EVAL_NNUE)
        FLD rfrom, rto;
#endif
        if (mv == MOVE_O_O[side]) {
#if defined(EVAL_NNUE)
            assert(castling);
            rto = FX[side];
            rfrom = HX[side];
#endif
            Remove(FX[side]);
            Put(HX[side], ROOK | side);
        }
        else if (mv == MOVE_O_O_O[side]) {
#if defined(EVAL_NNUE)
            assert(castling);
            rto = DX[side];
            rfrom = AX[side];
#endif
            Remove(DX[side]);
            Put(AX[side], ROOK | side);
        }

#if defined(EVAL_NNUE)
        if (castling) {
            auto & dp = m_state->dirtyPiece;
            PieceId dp0, dp1;
            dp.dirty_num = 2; // 2 pieces moved

            dp0 = piece_id_on(to);
            dp1 = piece_id_on(rto);
            evalList.put_piece(dp0, from, side == WHITE ? W_KING : B_KING);
            evalList.put_piece(dp1, rfrom, side == WHITE ? W_ROOK : B_ROOK);
        }
#endif
        break;
    default:
        break;
    }

    --m_ply;
    m_side ^= 1;

#if defined(EVAL_NNUE)
    m_state = undo.previous;
    if (!m_state)
        m_state = &m_undos[0];
#endif
}

void Position::MakeNullMove()
{
    Undo & undo = m_undos[m_undoSize++];
    undo.m_castlings = m_castlings;
    undo.m_ep = m_ep;
    undo.m_fifty = m_fifty;
    undo.m_hash = Hash();
    undo.m_mv = 0;

#if defined(EVAL_NNUE)
    undo.previous = m_undoSize == 1 ? nullptr : m_state;
    m_state = &undo;
    m_state->accumulator.computed_score = false;
#endif

    m_ep = NF;

    ++m_ply;
    m_side ^= 1;
}

void Position::UnmakeNullMove()
{
    if (m_undoSize == 0)
        return;

    const Undo& undo = m_undos[--m_undoSize];
    m_castlings = undo.m_castlings;
    m_ep = undo.m_ep;
    m_fifty = undo.m_fifty;

    --m_ply;
    m_side ^= 1;

#if defined(EVAL_NNUE)
    m_state = undo.previous;

    if (!m_state)
        m_state = &m_undos[0];
#endif
}

void Position::MovePiece(PIECE p, FLD from, FLD to)
{
    assert(from >= 0 && from < 64);
    assert(to >= 0 && to < 64);
    assert(p == m_board[from]);

    COLOR side = GetColor(p);

    m_bits[p] ^= BB_SINGLE[from];
    m_bits[p] ^= BB_SINGLE[to];

    m_bitsAll[side] ^= BB_SINGLE[from];
    m_bitsAll[side] ^= BB_SINGLE[to];

    m_board[from] = NOPIECE;
    m_board[to] = p;

    m_hash ^= s_hash[from][p];
    m_hash ^= s_hash[to][p];

    m_pawnHash ^= s_pawnHash[from][p];
    m_pawnHash ^= s_pawnHash[to][p];

    m_score += pieceSquareTables[p][to];
    m_score -= pieceSquareTables[p][from];
}

void Position::Print() const
{
    const char names[] = "-?PpNnBbRrQqKk";

    std::cout << std::endl;
    for (FLD f = 0; f < 64; ++f)
    {
        PIECE p = m_board[f];

        std::cout << " " << names[p];

        if (Col(f) == 7)
            std::cout << std::endl;
    }
    std::cout << std::endl;

    if (m_undoSize > 0)
    {
        for (int i = 0; i < m_undoSize; ++i)
            std::cout << " " << MoveToStrLong(m_undos[i].m_mv);
        std::cout << std::endl << std::endl;
    }
}

#if defined(EVAL_NNUE)
inline Piece Position::NNUEPieceAdaptor(PIECE p)
{
    Piece t = NO_PIECE;

    switch (p)
    {
    case PW:
        t = W_PAWN;
        break;
    case NW:
        t = W_KNIGHT;
        break;
    case BW:
        t = W_BISHOP;
        break;
    case RW:
        t = W_ROOK;
        break;
    case QW:
        t = W_QUEEN;
        break;
    case KW:
        t = W_KING;
        break;
    case PB:
        t = B_PAWN;
        break;
    case NB:
        t = B_KNIGHT;
        break;
    case BB:
        t = B_BISHOP;
        break;
    case RB:
        t = B_ROOK;
        break;
    case QB:
        t = B_QUEEN;
        break;
    case KB:
        t = B_KING;
        break;
    }

    return t;
}
#endif

void Position::Put(FLD f, PIECE p)
{
    assert(f >= 0 && f < 64);
    assert(p >= 2 && p < 14);

    COLOR side = GetColor(p);
    m_bits[p] ^= BB_SINGLE[f];
    m_bitsAll[side] ^= BB_SINGLE[f];
    m_board[f] = p;

    m_hash ^= s_hash[f][p];
    m_pawnHash ^= s_pawnHash[f][p];
    m_matIndex[side] += s_matIndexDelta[p];
    ++m_count[p];

    m_score += pieceSquareTables[p][f];
}

#if defined(EVAL_NNUE)
void Position::Put(FLD f, PIECE p, PieceId & next_piece_id)
{
    assert(f >= 0 && f < 64);
    assert(p >= 2 && p < 14);

    COLOR side = GetColor(p);
    m_bits[p] ^= BB_SINGLE[f];
    m_bitsAll[side] ^= BB_SINGLE[f];
    m_board[f] = p;

    m_hash ^= s_hash[f][p];
    m_pawnHash ^= s_pawnHash[f][p];
    m_matIndex[side] += s_matIndexDelta[p];
    ++m_count[p];

    m_score += pieceSquareTables[p][f];

    PieceId piece_id;

    Piece t = NO_PIECE;

    switch (p)
    {
    case PW:
        t = W_PAWN;
        break;
    case NW:
        t = W_KNIGHT;
        break;
    case BW:
        t = W_BISHOP;
        break;
    case RW:
        t = W_ROOK;
        break;
    case QW:
        t = W_QUEEN;
        break;
    case KW:
        t = W_KING;
        break;
    case PB:
        t = B_PAWN;
        break;
    case NB:
        t = B_KNIGHT;
        break;
    case BB:
        t = B_BISHOP;
        break;
    case RB:
        t = B_ROOK;
        break;
    case QB:
        t = B_QUEEN;
        break;
    case KB:
        t = B_KING;
        break;
    }

    piece_id =
        (p == KW) ? PIECE_ID_WKING :
        (p == KB) ? PIECE_ID_BKING :
        next_piece_id++;

    evalList.put_piece(piece_id, f, t);
}
#endif

void Position::Remove(FLD f)
{
    assert(f >= 0 && f < 64);
    PIECE p = m_board[f];
    assert(p >= 2 && p < 14);

    COLOR side = GetColor(p);
    m_bits[p] ^= BB_SINGLE[f];
    m_bitsAll[side] ^= BB_SINGLE[f];
    m_board[f] = NOPIECE;

    m_hash ^= s_hash[f][p];
    m_pawnHash ^= s_pawnHash[f][p];
    m_matIndex[side] -= s_matIndexDelta[p];
    --m_count[p];

    m_score -= pieceSquareTables[p][f];
}

int Position::Repetitions() const
{
    int r = 1;
    U64 hash0 = Hash();

    for (int i = m_undoSize - 1; i >= 0; --i)
    {
        if (m_undos[i].m_hash == hash0)
            ++r;

        Move mv = m_undos[i].m_mv;
        if (mv == 0)
            break;
        if (mv.Captured())
            break;
        if (mv.Piece() <= PB)
            break;
    }
    return r;
}

bool Position::SetFEN(const std::string& fen)
{
    m_initialPosition = (fen == STD_POSITION);

    if (fen.length() < 5) {
        std::cout << "Invalid fen " << fen << std::endl;
        return false;
    }

    Clear();

    std::vector<std::string> tokens;
    Split(fen, tokens);

    FLD f = A8;
#if defined(EVAL_NNUE)
    PieceId next_piece_id = PIECE_ID_ZERO;
#endif

    for (size_t i = 0; i < tokens[0].length(); ++i)
    {
        PIECE p = NOPIECE;
        char ch = tokens[0][i];
        switch (ch)
        {
            case 'P': p = PW; break;
            case 'N': p = NW; break;
            case 'B': p = BW; break;
            case 'R': p = RW; break;
            case 'Q': p = QW; break;
            case 'K': p = KW; break;

            case 'p': p = PB; break;
            case 'n': p = NB; break;
            case 'b': p = BB; break;
            case 'r': p = RB; break;
            case 'q': p = QB; break;
            case 'k': p = KB; break;

            case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8':
                f += ch - '0';
                break;

            case '/':
                if (Col(f) != 0)
                    f = 8 * (Row(f) + 1);
                break;

            default:
                goto ILLEGAL_FEN;
        }

        if (p)
        {
            if (f >= 64)
                goto ILLEGAL_FEN;
            if (p == KW)
                m_Kings[WHITE] = f;
            else if (p == KB)
                m_Kings[BLACK] = f;
#if defined(EVAL_NNUE)
            Put(f++, p, next_piece_id);
#else
            Put(f++, p);
#endif
        }
    }

    if (tokens.size() < 2)
        goto FINAL_CHECK;
    if (tokens[1] == "w")
        m_side = WHITE;
    else if (tokens[1] == "b")
        m_side = BLACK;
    else
        goto ILLEGAL_FEN;

    if (tokens.size() < 3)
        goto FINAL_CHECK;
    for (size_t i = 0; i < tokens[2].length(); ++i)
    {
        switch (tokens[2][i])
        {
        case 'K':
            if (m_board[E1] == KW && m_board[H1] == RW)
                m_castlings |= CASTLINGS[WHITE][KINGSIDE];
            break;
        case 'Q':
            if (m_board[E1] == KW && m_board[A1] == RW)
                m_castlings |= CASTLINGS[WHITE][QUEENSIDE];
            break;
        case 'k':
            if (m_board[E8] == KB && m_board[H8] == RB)
                m_castlings |= CASTLINGS[BLACK][KINGSIDE];
            break;
        case 'q':
            if (m_board[E8] == KB && m_board[A8] == RB)
                m_castlings |= CASTLINGS[BLACK][QUEENSIDE];
            break;
        case '-': break;
        default: goto ILLEGAL_FEN;
        }
    }

    if (tokens.size() < 4)
        goto FINAL_CHECK;
    if (tokens[3] != "-")
    {
        m_ep = StrToFld(tokens[3]);
        if (m_ep == NF)
            goto ILLEGAL_FEN;
    }

    if (tokens.size() < 5)
        goto FINAL_CHECK;
    m_fifty = atoi(tokens[4].c_str());
    if (m_fifty < 0)
        m_fifty = 0;

    if (tokens.size() < 6)
        goto FINAL_CHECK;
    m_ply = (atoi(tokens[5].c_str()) - 1) * 2 + m_side;
    if (m_ply < 0)
        m_ply = 0;

FINAL_CHECK:

    if (m_count[KW] != 1 || m_count[KB] != 1)
        goto ILLEGAL_FEN;

    if (m_ep != NF)
    {
        if (m_side == WHITE && Row(m_ep) != 2)
            goto ILLEGAL_FEN;
        if (m_side == BLACK && Row(m_ep) != 5)
            goto ILLEGAL_FEN;
    }

    if (m_bits[PW] & (BB_HORIZONTAL[0] | BB_HORIZONTAL[7]))
        goto ILLEGAL_FEN;
    if (m_bits[PB] & (BB_HORIZONTAL[0] | BB_HORIZONTAL[7]))
        goto ILLEGAL_FEN;

    return true;

ILLEGAL_FEN:

    //*this = *tmp.get();
    std::cout << "Invalid fen " << fen << std::endl;
    return false;
}

void Position::SetInitial()
{
    SetFEN(STD_POSITION);
}

bool Position::isInitialPosition()
{
    return m_initialPosition;
}

bool Position::NonPawnMaterial()
{
    COLOR side = Side();

    if (Bits(QUEEN | side))
        return true;

    if (Bits(ROOK | side))
        return true;

    if (Bits(BISHOP | side))
        return true;

    if (Bits(KNIGHT | side))
        return true;

    return false;
}

EVAL Position::nonPawnMaterial()
{
    return nonPawnMaterial(WHITE)
        +  nonPawnMaterial(BLACK);
}

EVAL Position::nonPawnMaterial(COLOR side)
{
    return (Count(KNIGHT | side) * VAL_N)
        +  (Count(BISHOP | side) * VAL_B)
        +  (Count(ROOK   | side) * VAL_R)
        +  (Count(QUEEN  | side) * VAL_Q);
}

#if defined(EVAL_NNUE)
const EvalList * Position::eval_list() const {
    return &evalList;
}
#endif