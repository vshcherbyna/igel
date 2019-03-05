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

#include "moves.h"

MoveList::MoveList()
{
    Clear();
}

bool MoveList::Swap(size_t i, size_t j)
{
    if (i > 255 || j > 255)
        return false;

    std::swap(m_data[i], m_data[j]);
    return true;
}

bool MoveList::Add(Move mv)
{
    if (overflow())
        return false;

    m_data[m_size++].m_mv = mv;
    return true;
}

bool MoveList::Add(FLD from, FLD to, PIECE piece)
{
    if (overflow())
        return false;

    m_data[m_size++].m_mv = Move(from, to, piece);
    return true;
}

bool MoveList::Add(FLD from, FLD to, PIECE piece, PIECE captured)
{
    if (overflow())
        return false;

    m_data[m_size++].m_mv = Move(from, to, piece, captured);
    return true;
}

bool MoveList::Add(FLD from, FLD to, PIECE piece, PIECE captured, PIECE promotion)
{
    if (overflow())
        return false;

    m_data[m_size++].m_mv = Move(from, to, piece, captured, promotion);
    return true;
}

bool GenAllMoves(const Position& pos, MoveList& mvlist)
{
    mvlist.Clear();

    COLOR side = pos.Side();
    COLOR opp = side ^ 1;
    U64 occ = pos.BitsAll();
    U64 freeOrOpp = ~pos.BitsAll(side);

    PIECE piece, captured;
    U64 x, y;
    FLD from, to;
    bool overflow = false;

    //
    //   PAWNS
    //

    int fwd = -8 + 16 * side;
    int second = 6 - 5 * side;
    int seventh = 1 + 5 * side;

    piece = PW | side;
    x = pos.Bits(piece);
    while (x)
    {
        from = PopLSB(x);
        int row = Row(from);

        to = from + fwd;
        if (!pos[to])
        {
            if (row == second)
            {
                overflow = mvlist.Add(from, to, piece);
                to += fwd;
                if (!pos[to])
                    overflow = mvlist.Add(from, to, piece);
            }
            else if (row == seventh)
            {
                overflow = mvlist.Add(from, to, piece, NOPIECE, QW | side);
                overflow = mvlist.Add(from, to, piece, NOPIECE, RW | side);
                overflow = mvlist.Add(from, to, piece, NOPIECE, BW | side);
                overflow = mvlist.Add(from, to, piece, NOPIECE, NW | side);
            }
            else
                overflow = mvlist.Add(from, to, piece);
        }

        y = BB_PAWN_ATTACKS[from][side] & pos.BitsAll(opp);
        while (y)
        {
            to = PopLSB(y);
            captured = pos[to];
            if (row == seventh)
            {
                overflow = mvlist.Add(from, to, piece, captured, QW | side);
                overflow = mvlist.Add(from, to, piece, captured, RW | side);
                overflow = mvlist.Add(from, to, piece, captured, BW | side);
                overflow = mvlist.Add(from, to, piece, captured, NW | side);
            }
            else
                overflow = mvlist.Add(from, to, piece, captured);
        }
    }

    if (pos.EP() != NF)
    {
        to = pos.EP();
        y = BB_PAWN_ATTACKS[to][opp] & pos.Bits(piece);
        while (y)
        {
            from = PopLSB(y);
            captured = piece ^ 1;
            overflow = mvlist.Add(from, to, piece, captured);
        }
    }

    //
    //   KNIGHTS
    //
    
    piece = KNIGHT | side;
    x = pos.Bits(piece);
    while (x)
    {
        from = PopLSB(x);
        y = BB_KNIGHT_ATTACKS[from] & freeOrOpp;
        while (y)
        {
            to = PopLSB(y);
            captured = pos[to];
            overflow = mvlist.Add(from, to, piece, captured);
        }
    }

    //
    //   BISHOPS
    //
    
    piece = BISHOP | side;
    x = pos.Bits(piece);
    while (x)
    {
        from = PopLSB(x);
        y = BishopAttacks(from, occ) & freeOrOpp;
        while (y)
        {
            to = PopLSB(y);
            captured = pos[to];
            overflow = mvlist.Add(from, to, piece, captured);
        }
    }

    //
    //   ROOKS
    //
    
    piece = ROOK | side;
    x = pos.Bits(piece);
    while (x)
    {
        from = PopLSB(x);
        y = RookAttacks(from, occ) & freeOrOpp;
        while (y)
        {
            to = PopLSB(y);
            captured = pos[to];
            overflow = mvlist.Add(from, to, piece, captured);
        }
    }

    //
    //   QUEENS
    //
    
    piece = QUEEN | side;
    x = pos.Bits(piece);
    while (x)
    {
        from = PopLSB(x);
        y = QueenAttacks(from, occ) & freeOrOpp;
        while (y)
        {
            to = PopLSB(y);
            captured = pos[to];
            overflow = mvlist.Add(from, to, piece, captured);
        }
    }

    //
    //   KINGS
    //

    piece = KING | side;
    from = pos.King(side);
    y = BB_KING_ATTACKS[from] & freeOrOpp;
    while (y)
    {
        to = PopLSB(y);
        captured = pos[to];
        overflow = mvlist.Add(from, to, piece, captured);
    }

    // castlings
    if (pos.CanCastle(side, KINGSIDE))
        overflow = mvlist.Add(MOVE_O_O[side]);

    if (pos.CanCastle(side, QUEENSIDE))
        overflow = mvlist.Add(MOVE_O_O_O[side]);

    return overflow;
}

void GenCapturesAndPromotions(const Position& pos, MoveList& mvlist)
{
    mvlist.Clear();

    COLOR side = pos.Side();
    COLOR opp = side ^ 1;
    U64 occ = pos.BitsAll();
    U64 targets = pos.BitsAll(opp);

    PIECE piece, captured;
    U64 x, y;
    FLD from, to;

    //
    //   PAWNS
    //

    int fwd = -8 + 16 * side;
    int seventh = 1 + 5 * side;

    piece = PW | side;
    x = pos.Bits(piece);
    while (x)
    {
        from = PopLSB(x);
        int row = Row(from);

        to = from + fwd;
        if (!pos[to])
        {
            if (row == seventh)
                mvlist.Add(from, to, piece, NOPIECE, QW | side);
        }

        y = BB_PAWN_ATTACKS[from][side] & pos.BitsAll(opp);
        while (y)
        {
            to = PopLSB(y);
            captured = pos[to];
            if (row == seventh)
                mvlist.Add(from, to, piece, captured, QW | side);
            else
                mvlist.Add(from, to, piece, captured);
        }
    }

    if (pos.EP() != NF)
    {
        to = pos.EP();
        y = BB_PAWN_ATTACKS[to][opp] & pos.Bits(piece);
        while (y)
        {
            from = PopLSB(y);
            captured = piece ^ 1;
            mvlist.Add(from, to, piece, captured);
        }
    }

    //
    //  Menace high value pieces first
    //

    //
    //   KINGS
    //

    piece = KING | side;
    from = pos.King(side);
    y = BB_KING_ATTACKS[from] & targets;
    while (y)
    {
        to = PopLSB(y);
        captured = pos[to];
        mvlist.Add(from, to, piece, captured);
    }

    //
    //   QUEENS
    //

    piece = QUEEN | side;
    x = pos.Bits(piece);
    while (x)
    {
        from = PopLSB(x);
        y = QueenAttacks(from, occ) & targets;
        while (y)
        {
            to = PopLSB(y);
            captured = pos[to];
            mvlist.Add(from, to, piece, captured);
        }
    }

    //
    //   ROOKS
    //
    
    piece = ROOK | side;
    x = pos.Bits(piece);
    while (x)
    {
        from = PopLSB(x);
        y = RookAttacks(from, occ) & targets;
        while (y)
        {
            to = PopLSB(y);
            captured = pos[to];
            mvlist.Add(from, to, piece, captured);
        }
    }

    //
    //   BISHOPS
    //
    
    piece = BISHOP | side;
    x = pos.Bits(piece);
    while (x)
    {
        from = PopLSB(x);
        y = BishopAttacks(from, occ) & targets;
        while (y)
        {
            to = PopLSB(y);
            captured = pos[to];
            mvlist.Add(from, to, piece, captured);
        }
    }

    //
    //   KNIGHTS
    //
    
    piece = KNIGHT | side;
    x = pos.Bits(piece);
    while (x)
    {
        from = PopLSB(x);
        y = BB_KNIGHT_ATTACKS[from] & targets;
        while (y)
        {
            to = PopLSB(y);
            captured = pos[to];
            mvlist.Add(from, to, piece, captured);
        }
    }
}

void AddSimpleChecks(const Position& pos, MoveList& mvlist)
{
    COLOR side = pos.Side();
    COLOR opp = side ^ 1;

    FLD K = pos.King(opp);
    U64 occ = pos.BitsAll();
    U64 free = ~occ;

    PIECE piece;
    U64 x, y;
    FLD from, to;

    U64 zoneN = BB_KNIGHT_ATTACKS[K] & free;
    U64 zoneB = BB_BISHOP_ATTACKS[K] & free;
    U64 zoneR = BB_ROOK_ATTACKS[K] & free;
    U64 zoneQ = BB_QUEEN_ATTACKS[K] & free;

    //
    //   KNIGHTS
    //

    piece = KNIGHT | side;
    x = pos.Bits(piece);
    while (x)
    {
        from = PopLSB(x);
        y = BB_KNIGHT_ATTACKS[from] & zoneN;
        while (y)
        {
            to = PopLSB(y);
            mvlist.Add(from, to, piece);
        }
    }

    //
    //   BISHOPS
    //

    piece = BISHOP | side;
    x = pos.Bits(piece);
    while (x)
    {
        from = PopLSB(x);
        y = BB_BISHOP_ATTACKS[from] & zoneB;
        while (y)
        {
            to = PopLSB(y);
            if ((BB_BETWEEN[from][to] & occ) == 0)
                mvlist.Add(from, to, piece);
        }
    }

    //
    //   ROOKS
    //

    piece = ROOK | side;
    x = pos.Bits(piece);
    while (x)
    {
        from = PopLSB(x);
        y = BB_ROOK_ATTACKS[from] & zoneR;
        while (y)
        {
            to = PopLSB(y);
            if ((BB_BETWEEN[from][to] & occ) == 0)
                mvlist.Add(from, to, piece);
        }
    }

    //
    //   QUEENS
    //

    piece = QUEEN | side;
    x = pos.Bits(piece);
    while (x)
    {
        from = PopLSB(x);
        y = BB_QUEEN_ATTACKS[from] & zoneQ;
        while (y)
        {
            to = PopLSB(y);
            if ((BB_BETWEEN[from][to] & occ) == 0)
                mvlist.Add(from, to, piece);
        }
    }
}

U64 GetCheckMask(const Position& pos)
{
    COLOR side = pos.Side();
    COLOR opp = side ^ 1;
    FLD K = pos.King(side);

    U64 x, occ = pos.BitsAll();
    U64 mask = 0;
    FLD from;

    x = BB_PAWN_ATTACKS[K][side] & pos.Bits(PW | opp);
    mask |= x;

    x = BB_KNIGHT_ATTACKS[K] & pos.Bits(NW | opp);
    mask |= x;

    x = BishopAttacks(K, occ) & (pos.Bits(BW | opp) | pos.Bits(QW | opp));
    while (x)
    {
        from = PopLSB(x);
        mask |= BB_SINGLE[from];
        mask |= BB_BETWEEN[K][from];
    }

    x = RookAttacks(K, occ) & (pos.Bits(RW | opp) | pos.Bits(QW | opp));
    while (x)
    {
        from = PopLSB(x);
        mask |= BB_SINGLE[from];
        mask |= BB_BETWEEN[K][from];
    }

    return mask;
}

void GenMovesInCheck(const Position& pos, MoveList& mvlist)
{
    mvlist.Clear();

    COLOR side = pos.Side();
    COLOR opp = side ^ 1;
    U64 occ = pos.BitsAll();
    U64 freeOrOpp = ~pos.BitsAll(side);

    PIECE piece, captured;
    U64 x, y;
    FLD from, to;

    U64 checkMask = GetCheckMask(pos);

    //
    //   KINGS
    //

    piece = KING | side;
    from = pos.King(side);
    y = BB_KING_ATTACKS[from] & freeOrOpp;
    while (y)
    {
        to = PopLSB(y);
        captured = pos[to];
        if (captured || !(BB_SINGLE[to] & checkMask))
            mvlist.Add(from, to, piece, captured);
    }

    //
    //   PAWNS
    //

    int fwd = -8 + 16 * side;
    int second = 6 - 5 * side;
    int seventh = 1 + 5 * side;

    piece = PW | side;
    x = pos.Bits(piece);
    while (x)
    {
        from = PopLSB(x);
        int row = Row(from);

        to = from + fwd;
        if (!pos[to])
        {
            if (row == second)
            {
                if (BB_SINGLE[to] & checkMask)
                    mvlist.Add(from, to, piece);
                to += fwd;
                if (!pos[to])
                    if (BB_SINGLE[to] & checkMask)
                        mvlist.Add(from, to, piece);
            }
            else if (row == seventh)
            {
                if (BB_SINGLE[to] & checkMask)
                {
                    mvlist.Add(from, to, piece, NOPIECE, QW | side);
                    mvlist.Add(from, to, piece, NOPIECE, RW | side);
                    mvlist.Add(from, to, piece, NOPIECE, BW | side);
                    mvlist.Add(from, to, piece, NOPIECE, NW | side);
                }
            }
            else
            {
                if (BB_SINGLE[to] & checkMask)
                    mvlist.Add(from, to, piece);
            }
        }

        y = BB_PAWN_ATTACKS[from][side] & pos.BitsAll(opp);
        while (y)
        {
            to = PopLSB(y);
            if (BB_SINGLE[to] & checkMask)
            {
                captured = pos[to];
                if (row == seventh)
                {
                    mvlist.Add(from, to, piece, captured, QW | side);
                    mvlist.Add(from, to, piece, captured, RW | side);
                    mvlist.Add(from, to, piece, captured, BW | side);
                    mvlist.Add(from, to, piece, captured, NW | side);
                }
                else
                    mvlist.Add(from, to, piece, captured);
            }
        }
    }

    if (pos.EP() != NF)
    {
        to = pos.EP();
        y = BB_PAWN_ATTACKS[to][opp] & pos.Bits(piece);
        while (y)
        {
            from = PopLSB(y);
            captured = piece ^ 1;
            mvlist.Add(from, to, piece, captured);
        }
    }

    //
    //   KNIGHTS
    //
    
    piece = KNIGHT | side;
    x = pos.Bits(piece);
    while (x)
    {
        from = PopLSB(x);
        y = BB_KNIGHT_ATTACKS[from] & freeOrOpp & checkMask;
        while (y)
        {
            to = PopLSB(y);
            captured = pos[to];
            mvlist.Add(from, to, piece, captured);
        }
    }

    //
    //   BISHOPS
    //
    
    piece = BISHOP | side;
    x = pos.Bits(piece);
    while (x)
    {
        from = PopLSB(x);
        y = BishopAttacks(from, occ) & freeOrOpp & checkMask;
        while (y)
        {
            to = PopLSB(y);
            captured = pos[to];
            mvlist.Add(from, to, piece, captured);
        }
    }

    //
    //   ROOKS
    //
    
    piece = ROOK | side;
    x = pos.Bits(piece);
    while (x)
    {
        from = PopLSB(x);
        y = RookAttacks(from, occ) & freeOrOpp & checkMask;
        while (y)
        {
            to = PopLSB(y);
            captured = pos[to];
            mvlist.Add(from, to, piece, captured);
        }
    }

    //
    //   QUEENS
    //
    
    piece = QUEEN | side;
    x = pos.Bits(piece);
    while (x)
    {
        from = PopLSB(x);
        y = QueenAttacks(from, occ) & freeOrOpp & checkMask;
        while (y)
        {
            to = PopLSB(y);
            captured = pos[to];
            mvlist.Add(from, to, piece, captured);
        }
    }
}

