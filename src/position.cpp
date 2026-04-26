/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2002-2018 Vladimir Medvedev <vrm@bk.ru> (GreKo author)
*  Copyright (C) 2018-2025 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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
#include "nnue.h"

#include <memory>

const Move MOVE_O_O[2]   = { Move(E1, G1, KW), Move(E8, G8, KB) };
const Move MOVE_O_O_O[2] = { Move(E1, C1, KW), Move(E8, C8, KB) };

bool g_uci_chess960 = false;

U64 Position::s_hash[64][14];
U64 Position::s_hashSide[2];
U64 Position::s_hashCastlings[256];
U64 Position::s_hashEP[256];

const int Position::s_matIndexDelta[14] = { 0, 0, 0, 0, 3, 3, 3, 3, 5, 5, 10, 10, 0, 0 };
static Piece PieceAdapter[] = { NO_PIECE, NO_PIECE, W_PAWN, B_PAWN, W_KNIGHT, B_KNIGHT, W_BISHOP, B_BISHOP, W_ROOK, B_ROOK, W_QUEEN, B_QUEEN, W_KING, B_KING };

bool Position::CanCastle(COLOR side, U8 flank) const
{
    if (InCheck())
        return false;
    if ((m_castlings & CASTLINGS[side][flank]) == 0)
        return false;

    COLOR opp = side ^ 1;

    FLD kingFrom = m_Kings[side];
    FLD rookFrom = m_castlingRookSq[side][flank];

    if (rookFrom == NF)
        return false;

    int rankBase = (side == WHITE) ? A1 : A8;
    FLD kingTo   = (flank == KINGSIDE) ? (FLD)(rankBase + 6) : (FLD)(rankBase + 2);
    FLD rookTo   = (flank == KINGSIDE) ? (FLD)(rankBase + 5) : (FLD)(rankBase + 3);

    int kingMin = (kingFrom < kingTo) ? kingFrom : kingTo;
    int kingMax = (kingFrom > kingTo) ? kingFrom : kingTo;
    int rookMin = (rookFrom < rookTo) ? rookFrom : rookTo;
    int rookMax = (rookFrom > rookTo) ? rookFrom : rookTo;
    int spanMin = (kingMin < rookMin) ? kingMin  : rookMin;
    int spanMax = (kingMax > rookMax) ? kingMax  : rookMax;

    for (int s = spanMin; s <= spanMax; ++s) {
        if (s == kingFrom || s == rookFrom)
            continue;
        if (m_board[s])
            return false;
    }

    int kStep = (kingTo > kingFrom) ? 1 : (kingTo < kingFrom) ? -1 : 0;
    if (kStep != 0) {
        for (int s = (int)kingFrom + kStep; ; s += kStep) {
            if (IsAttacked((FLD)s, opp))
                return false;
            if (s == (int)kingTo)
                break;
        }
    }

    return true;
}

void Position::Clear()
{
    std::memset(evalList.piece_id_list, 0, sizeof(evalList.piece_id_list));
    std::memset(evalList.pieceListFw,   0, sizeof(evalList.pieceListFw));
    std::memset(evalList.pieceListFb,   0, sizeof(evalList.pieceListFb));

    for (unsigned int i = 0; i < sizeof(m_undos) / sizeof(Undo); ++i)
        m_undos->reset();

    m_state = &m_undos[0];

    for (FLD f = 0; f < 64; ++f)
        m_board[f] = NOPIECE;

    for (PIECE p = 0; p < 14; ++p)
    {
        m_bits[p] = 0;
        m_count[p] = 0;
    }

    m_bitsAll[WHITE] = m_bitsAll[BLACK] = 0;
    m_castlings = 0;
    m_castlingRookSq[WHITE][KINGSIDE]  = NF;
    m_castlingRookSq[WHITE][QUEENSIDE] = NF;
    m_castlingRookSq[BLACK][KINGSIDE]  = NF;
    m_castlingRookSq[BLACK][QUEENSIDE] = NF;
    m_ep = NF;
    m_fifty = 0;
    m_hash = 0;
    m_Kings[WHITE] = m_Kings[BLACK] = NF;
    m_matIndex[WHITE] = m_matIndex[BLACK] = 0;
    m_ply = 0;
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
        if (g_uci_chess960)
        {
            if (m_castlings & CASTLINGS[WHITE][KINGSIDE])
                fen << (char)('A' + Col(m_castlingRookSq[WHITE][KINGSIDE]));
            if (m_castlings & CASTLINGS[WHITE][QUEENSIDE])
                fen << (char)('A' + Col(m_castlingRookSq[WHITE][QUEENSIDE]));
            if (m_castlings & CASTLINGS[BLACK][KINGSIDE])
                fen << (char)('a' + Col(m_castlingRookSq[BLACK][KINGSIDE]));
            if (m_castlings & CASTLINGS[BLACK][QUEENSIDE])
                fen << (char)('a' + Col(m_castlingRookSq[BLACK][QUEENSIDE]));
        }
        else
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



void Position::InitHashNumbers()
{
    RandSeed(30147);

    for (FLD f = 0; f < 64; ++f) {
        for (PIECE p = 0; p < 14; ++p) {
            s_hash[f][p] = Rand64();
        }
    }

    s_hashSide[WHITE] = Rand64();
    s_hashSide[BLACK] = Rand64();

    for (int i = 0; i < 256; ++i) {
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

    U64 occ = BitsAll();

    if (BishopAttacks(f, occ) & (Bits(BISHOP | side) | Bits(QUEEN | side)))
        return true;

    if (RookAttacks(f, occ) & (Bits(ROOK | side) | Bits(QUEEN | side)))
        return true;

    return false;
}

inline PieceId Position::piece_id_on(Square sq) const
{
    Square flipped = FLIP[BLACK][sq];
    PieceId pid = evalList.piece_id_list[flipped];
    assert(is_ok(pid));

    return pid;
}

bool Position::MakeMove(Move mv)
{
    assert(m_undoSize < MAX_UNDO);
    Undo & undo = m_undos[m_undoSize++];

    undo.m_castlings = m_castlings;
    undo.m_castlingRookSq[WHITE][KINGSIDE]  = m_castlingRookSq[WHITE][KINGSIDE];
    undo.m_castlingRookSq[WHITE][QUEENSIDE] = m_castlingRookSq[WHITE][QUEENSIDE];
    undo.m_castlingRookSq[BLACK][KINGSIDE]  = m_castlingRookSq[BLACK][KINGSIDE];
    undo.m_castlingRookSq[BLACK][QUEENSIDE] = m_castlingRookSq[BLACK][QUEENSIDE];
    undo.m_ep = m_ep;
    undo.m_fifty = m_fifty;
    undo.m_hash = Hash();
    undo.m_mv = mv;

    undo.previous = m_undoSize == 1 ? nullptr : m_state;
    m_state = &undo;

    m_state->accumulator.computed_accumulation = false;
    m_state->accumulator.computed_score = false;
    PieceId dp0 = PIECE_ID_NONE;
    PieceId dp1 = PIECE_ID_NONE;
    auto & dp = m_state->dirtyPiece;
    dp.dirty_num = 1;

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
        auto nnueTo = to == m_ep ? to + 8 - 16 * side : to;
        m_fifty = 0;
        if (to == m_ep)
            Remove(to + 8 - 16 * side);
        else
            Remove(to);

        dp.dirty_num = 2; // 2 pieces moved
        dp1 = piece_id_on(nnueTo);
        dp.pieceId[1] = dp1;
        dp.old_piece[1] = evalList.piece_with_id(dp1);
        dp.old_piece[1].piece = PieceAdapter[captured];
        evalList.put_piece(dp1, to, NO_PIECE);
        dp.new_piece[1] = evalList.piece_with_id(dp1);
        dp.new_piece[1].piece = NO_PIECE;
    }

    auto castling = mv.IsCastling() || (mv == MOVE_O_O[side]) || (mv == MOVE_O_O_O[side]);

    // handle non-castling moves
    if (!castling) {
        dp0 = piece_id_on(from);
        dp.pieceId[0] = dp0;
        dp.old_piece[0] = evalList.piece_with_id(dp0);
        dp.old_piece[0].piece = PieceAdapter[piece];
        evalList.put_piece(dp0, to, PieceAdapter[piece]);
        dp.new_piece[0] = evalList.piece_with_id(dp0);
        dp.new_piece[0].piece = PieceAdapter[piece];
    }

    if (!castling)
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
                dp0 = piece_id_on(to);
                evalList.put_piece(dp0, to, PieceAdapter[promotion]);
                dp.new_piece[0] = evalList.piece_with_id(dp0);
                dp.new_piece[0].piece = PieceAdapter[promotion];
            }
            break;
        case KW:
        case KB:
            FLD rfrom, rto, kto;
            if (castling) {
                U8 flank;
                if (mv == MOVE_O_O[side]) {
                    flank = KINGSIDE;
                    rfrom = HX[side];
                }
                else if (mv == MOVE_O_O_O[side]) {
                    flank = QUEENSIDE;
                    rfrom = AX[side];
                }
                else {
                    rfrom = to; // FRC: to == rook-from
                    flank = (Col(rfrom) > Col(from)) ? KINGSIDE : QUEENSIDE;
                }
                int rankBase = (side == WHITE) ? A1 : A8;
                kto  = (flank == KINGSIDE) ? (FLD)(rankBase + 6) : (FLD)(rankBase + 2);
                rto  = (flank == KINGSIDE) ? (FLD)(rankBase + 5) : (FLD)(rankBase + 3);

                //
                // Capture piece ids before moving anything in piece_id_list.
                //

                dp0 = piece_id_on(from);
                dp1 = piece_id_on(rfrom);

                //
                // Remove king and rook from starting squares, then place them at destinations
                // Order handles FRC edge cases where destinations overlap with starts
                //

                Remove(from);
                Remove(rfrom);
                Put(kto, KING | side);
                Put(rto, ROOK | side);
                m_Kings[side] = kto;

                dp.dirty_num = 2; // 2 pieces moved
                dp.pieceId[0] = dp0;
                dp.old_piece[0] = evalList.piece_with_id(dp0);
                dp.old_piece[0].piece = side == WHITE ? W_KING : B_KING;
                evalList.put_piece(dp0, kto, side == WHITE ? W_KING : B_KING);
                dp.new_piece[0] = evalList.piece_with_id(dp0);
                dp.new_piece[0].piece = side == WHITE ? W_KING : B_KING;
                dp.pieceId[1] = dp1;
                dp.old_piece[1] = evalList.piece_with_id(dp1);
                dp.old_piece[1].piece = side == WHITE ? W_ROOK : B_ROOK;
                evalList.put_piece(dp1, rto, side == WHITE ? W_ROOK : B_ROOK);
                dp.new_piece[1] = evalList.piece_with_id(dp1);
                dp.new_piece[1].piece = side == WHITE ? W_ROOK : B_ROOK;
            }
            else {
                m_Kings[side] = to;
            }
            break;
        default:
            break;
    }

    //
    // King movement clears all castling rights for that side.
    //

    if (piece == (KING | WHITE))
        m_castlings &= ~(CASTLINGS[WHITE][KINGSIDE] | CASTLINGS[WHITE][QUEENSIDE]);
    else if (piece == (KING | BLACK))
        m_castlings &= ~(CASTLINGS[BLACK][KINGSIDE] | CASTLINGS[BLACK][QUEENSIDE]);

    // Movement from / capture on a rook starting square clears that flank.
    for (COLOR c = 0; c < 2; ++c) {
        for (U8 fl = 0; fl < 2; ++fl) {
            FLD rsq = m_castlingRookSq[c][fl];
            if (rsq != NF && (from == rsq || to == rsq))
                m_castlings &= ~CASTLINGS[c][fl];
        }
    }

    ++m_ply;
    m_side ^= 1;

    if (IsAttacked(King(side), opp)) {
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
    m_castlingRookSq[WHITE][KINGSIDE]  = undo.m_castlingRookSq[WHITE][KINGSIDE];
    m_castlingRookSq[WHITE][QUEENSIDE] = undo.m_castlingRookSq[WHITE][QUEENSIDE];
    m_castlingRookSq[BLACK][KINGSIDE]  = undo.m_castlingRookSq[BLACK][KINGSIDE];
    m_castlingRookSq[BLACK][QUEENSIDE] = undo.m_castlingRookSq[BLACK][QUEENSIDE];
    m_ep = undo.m_ep;
    m_fifty = undo.m_fifty;

    COLOR opp = m_side;
    COLOR side = opp ^ 1;

    auto castling = mv.IsCastling() || (mv == MOVE_O_O[side]) || (mv == MOVE_O_O_O[side]);

    if (castling) {
        // Determine the destinations the make-move placed king/rook on.
        FLD rfrom;
        U8 flank;
        if (mv == MOVE_O_O[side]) {
            flank = KINGSIDE;
            rfrom = HX[side];
        }
        else if (mv == MOVE_O_O_O[side]) {
            flank = QUEENSIDE;
            rfrom = AX[side];
        }
        else {
            rfrom = to; // FRC: to == rook-from-square
            flank = (Col(rfrom) > Col(from)) ? KINGSIDE : QUEENSIDE;
        }
        int rankBase = (side == WHITE) ? A1 : A8;
        FLD kto = (flank == KINGSIDE) ? (FLD)(rankBase + 6) : (FLD)(rankBase + 2);
        FLD rto = (flank == KINGSIDE) ? (FLD)(rankBase + 5) : (FLD)(rankBase + 3);

        //
        // Restore: remove king and rook from destinations, place them at starting squares
        //

        Remove(kto);
        if (rto != kto)
            Remove(rto);
        Put(from, KING | side);
        if (rfrom != from)
            Put(rfrom, ROOK | side);
        m_Kings[side] = from;

        auto & dp = m_state->dirtyPiece;
        PieceId dp0 = dp.pieceId[0];
        PieceId dp1 = dp.pieceId[1];
        evalList.put_piece(dp0, from,  side == WHITE ? W_KING : B_KING);
        evalList.put_piece(dp1, rfrom, side == WHITE ? W_ROOK : B_ROOK);
    }
    else {
        PieceId dp0 = m_state->dirtyPiece.pieceId[0];
        evalList.put_piece(dp0, from, PieceAdapter[piece]);

        Remove(to);

        if (captured) {
            auto nnueTo = to == m_ep ? to + 8 - 16 * side : to;
            if (to == m_ep)
                Put(to + 8 - 16 * side, captured);
            else
                Put(to, captured);
            PieceId dp1 = m_state->dirtyPiece.pieceId[1];
            assert(evalList.piece_with_id(dp1).from[WHITE] == PS_NONE);
            assert(evalList.piece_with_id(dp1).from[BLACK] == PS_NONE);
            evalList.put_piece(dp1, nnueTo, PieceAdapter[captured]);
        }

        Put(from, piece);

        if (piece == KW || piece == KB)
            m_Kings[side] = from;
    }

    --m_ply;
    m_side ^= 1;

    m_state = undo.previous;
    if (!m_state)
        m_state = &m_undos[0];
}

void Position::MakeNullMove()
{
    Undo & undo = m_undos[m_undoSize++];
    undo.m_castlings = m_castlings;
    undo.m_ep = m_ep;
    undo.m_fifty = m_fifty;
    undo.m_hash = Hash();
    undo.m_mv = 0;

    undo.previous = m_undoSize == 1 ? nullptr : m_state;
    m_state = &undo;
    m_state->accumulator.computed_score = false;
    m_state->accumulator.computed_accumulation = false;
    m_state->dirtyPiece.dirty_num = 0;
    m_state->dirtyPiece.pieceId[0] = PIECE_ID_NONE;

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

    m_state = undo.previous;

    if (!m_state)
        m_state = &m_undos[0];
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

void Position::Put(FLD f, PIECE p)
{
    assert(f >= 0 && f < 64);
    assert(p >= 2 && p < 14);

    COLOR side = GetColor(p);
    m_bits[p] ^= BB_SINGLE[f];
    m_bitsAll[side] ^= BB_SINGLE[f];
    m_board[f] = p;

    m_hash ^= s_hash[f][p];
    m_matIndex[side] += s_matIndexDelta[p];
    ++m_count[p];
}

void Position::Put(FLD f, PIECE p, PieceId & next_piece_id)
{
    assert(f >= 0 && f < 64);
    assert(p >= 2 && p < 14);

    COLOR side = GetColor(p);
    m_bits[p] ^= BB_SINGLE[f];
    m_bitsAll[side] ^= BB_SINGLE[f];
    m_board[f] = p;

    m_hash ^= s_hash[f][p];
    m_matIndex[side] += s_matIndexDelta[p];
    ++m_count[p];

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
    m_matIndex[side] -= s_matIndexDelta[p];
    --m_count[p];
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
    PieceId next_piece_id = PIECE_ID_ZERO;

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
            Put(f++, p, next_piece_id);
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
        char ch = tokens[2][i];
        if (ch == '-')
            continue;

        COLOR cside = (ch >= 'A' && ch <= 'Z') ? WHITE : BLACK;
        PIECE rookPiece = (cside == WHITE) ? RW : RB;
        PIECE kingPiece = (cside == WHITE) ? KW : KB;
        FLD kingSq = m_Kings[cside];
        if (kingSq == NF || m_board[kingSq] != kingPiece)
            goto ILLEGAL_FEN;
        int rankBase = (cside == WHITE) ? A1 : A8;
        char up = (ch >= 'a' && ch <= 'z') ? (char)(ch - 'a' + 'A') : ch;

        FLD rookSq = NF;
        U8 flank = 0;

        if (up == 'K') {
            // outermost rook to the right of the king
            for (int f = 7; f > Col(kingSq); --f) {
                FLD s = (FLD)(rankBase + f);
                if (m_board[s] == rookPiece) { rookSq = s; break; }
            }
            flank = KINGSIDE;
        }
        else if (up == 'Q') {
            // outermost rook to the left of the king
            for (int f = 0; f < Col(kingSq); ++f) {
                FLD s = (FLD)(rankBase + f);
                if (m_board[s] == rookPiece) { rookSq = s; break; }
            }
            flank = QUEENSIDE;
        }
        else if (up >= 'A' && up <= 'H') {
            // Shredder-FEN: explicit rook file
            int file = up - 'A';
            FLD s = (FLD)(rankBase + file);
            if (m_board[s] != rookPiece)
                goto ILLEGAL_FEN;
            rookSq = s;
            flank = (file > Col(kingSq)) ? KINGSIDE : QUEENSIDE;
        }
        else
        {
            goto ILLEGAL_FEN;
        }

        if (rookSq == NF)
            goto ILLEGAL_FEN;

        m_castlings |= CASTLINGS[cside][flank];
        m_castlingRookSq[cside][flank] = rookSq;
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

Move Position::getRandomMove()
{
    MoveList pseudo;
    GenAllMoves(*this, pseudo);
    std::vector<Move> moves;

    for (size_t i = 0; i < pseudo.Size(); ++i) {
        if (MakeMove(pseudo[i].m_mv)) {
            moves.push_back(pseudo[i].m_mv);
            UnmakeMove();
        }
    }

    if (moves.empty())
        return 0;
    else
        return moves[Rand32() % moves.size()];
}

const EvalList * Position::eval_list() const {
    return &evalList;
}

std::uint32_t Position::getActiveIndexes(COLOR c, std::uint32_t indexes[]) {
    const PieceId target = static_cast<PieceId>(PIECE_ID_KING + c);
    auto pieces = c == WHITE ? evalList.piece_list_fw() : evalList.piece_list_fb();
    Square kingSq = static_cast<Square>((pieces[target] - PS_KING) % SQUARE_NB);
    std::uint32_t count = 0;

    kingSq = FLIP[c][kingSq];

    // Precompute orient constants once (same logic as getChangedIndexes)
    const int flip_mask = (bool(c) * SQ_A8) ^ ((Col(kingSq) < FILE_E) * SQ_H1);
    const std::uint32_t king_bucket_offset = PS_END * KingBuckets[Square(int(kingSq) ^ flip_mask)];

    for (PieceId i = PIECE_ID_ZERO; i <= PIECE_ID_BKING; i++) {
        const auto ps = pieces[i];
        if (ps != PS_NONE) {
            const Square sq_raw = static_cast<Square>((ps - PS_KING) % SQUARE_NB);
            indexes[count++] = static_cast<std::uint32_t>(
                (int(FLIP[c][sq_raw]) ^ flip_mask)
                + PieceSquareIndex[c][PieceAdapter[m_board[FLIP[!c][sq_raw]]]]
                + king_bucket_offset);
        }
    }

    return count;
}

std::pair<std::uint32_t, std::uint32_t> Position::getChangedIndexes(COLOR c, std::uint32_t added[], std::uint32_t removed[]) {
    const PieceId target = static_cast<PieceId>(PIECE_ID_KING + c);
    auto pieces = c == WHITE ? evalList.piece_list_fw() : evalList.piece_list_fb();
    Square kingSq = static_cast<Square>((pieces[target] - PS_KING) % SQUARE_NB);

    kingSq = FLIP[c][kingSq];
    const auto & dp = state()->dirtyPiece;

    // Precompute once per call: orient(kingSq, kingSq, c) = kingSq ^ flip_mask
    const int flip_mask = (bool(c) * SQ_A8) ^ ((Col(kingSq) < FILE_E) * SQ_H1);
    const std::uint32_t king_bucket_offset = PS_END * KingBuckets[Square(int(kingSq) ^ flip_mask)];

    std::uint32_t ca = 0;
    std::uint32_t cr = 0;

    for (int i = 0; i < dp.dirty_num; ++i) {
        const auto old_p = static_cast<PieceSquare>(dp.old_piece[i].from[c]);

        if (old_p != PS_NONE) {
            Square sq = FLIP[c][static_cast<Square>((old_p - PS_KING) % SQUARE_NB)];
            removed[cr++] = static_cast<std::uint32_t>((int(sq) ^ flip_mask) + PieceSquareIndex[c][dp.old_piece[i].piece] + king_bucket_offset);
        }

        const auto new_p = static_cast<PieceSquare>(dp.new_piece[i].from[c]);

        if (new_p != PS_NONE) {
            Square sq = FLIP[c][static_cast<Square>((new_p - PS_KING) % SQUARE_NB)];
            added[ca++] = static_cast<std::uint32_t>((int(sq) ^ flip_mask) + PieceSquareIndex[c][dp.new_piece[i].piece] + king_bucket_offset);
        }
    }

    return { ca, cr };
}

Square orient(Square kingSq, Square s, COLOR c) {
    auto file = Col(kingSq);
    return Square(int(s) ^ (bool(c) * SQ_A8) ^ ((file < FILE_E) * SQ_H1));
}

inline std::uint32_t Position::makeIndex(Square sq_k, Square sq, Piece p, COLOR c) {
    Square o_ksq = orient(sq_k, sq_k, c);
    return static_cast<std::uint32_t>(orient(sq_k, sq, c) + PieceSquareIndex[c][p] + PS_END * KingBuckets[o_ksq]);
}