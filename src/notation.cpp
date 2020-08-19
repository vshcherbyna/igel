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

#include "moves.h"
#include "notation.h"

bool CanBeMove(const std::string& s)
{
    static const std::string goodChars = "12345678abcdefghNBRQKOxnbrq=-+#!?";
    for (size_t i = 0; i < s.length(); ++i)
    {
        if (goodChars.find_first_of(s[i]) == std::string::npos)
            return false;
    }
    return true;
}

std::string FldToStr(FLD f)
{
    char buf[3];
    buf[0] = 'a' + Col(f);
    buf[1] = '8' - Row(f);
    buf[2] = 0;
    return std::string(buf);
}

FLD StrToFld(const std::string& s)
{
    if (s.length() < 2)
        return NF;

    int col = s[0] - 'a';
    int row = '8' - s[1];
    if (col < 0 || col > 7 || row < 0 || row > 7)
        return NF;

    return 8 * row + col;
}

Move StrToMove(const std::string& s, Position& pos)
{
    std::string s1 = s;
    while (s1.length() > 2)
    {
        const std::string special = "+#?!";
        char ch = s1[s1.length() - 1];
        if (special.find_first_of(ch) == std::string::npos) break;
        s1 = s1.substr(0, s1.length() - 1);
    }

    MoveList mvlist;
    GenAllMoves(pos, mvlist);
    auto mvSize = mvlist.Size();

    for (size_t i = 0; i < mvSize; ++i)
    {
        Move mv = mvlist[i].m_mv;
        if (MoveToStrLong(mv) == s1)
        {
            return mv;
            /*if (pos.MakeMove(mv))
            {
                pos.UnmakeMove();
                return mv;
            }*/
        }
    }

    for (size_t i = 0; i < mvSize; ++i)
    {
        Move mv = mvlist[i].m_mv;
        if (MoveToStrShort(mv, pos, mvlist) == s1)
        {
            return mv;
            /*if (pos.MakeMove(mv))
            {
                pos.UnmakeMove();
                return mv;
            }*/
        }
    }

    return 0;
}

std::string MoveToStrLong(Move mv)
{
    std::string s = FldToStr(mv.From()) + FldToStr(mv.To());
    switch (mv.Promotion())
    {
        case QW: case QB: s += "q"; break;
        case RW: case RB: s += "r"; break;
        case BW: case BB: s += "b"; break;
        case NW: case NB: s += "n"; break;
        default: break;
    }
    return s;
}

std::string MoveToStrShort(Move mv, Position& pos, const MoveList& mvlist)
{
    if (mv == MOVE_O_O[WHITE] || mv == MOVE_O_O[BLACK])
        return "O-O";
    if (mv == MOVE_O_O_O[WHITE] || mv == MOVE_O_O_O[BLACK])
        return "O-O-O";

    std::string strPiece, strCapture, strFrom, strTo, strPromotion;
    FLD from = mv.From();
    FLD to = mv.To();
    PIECE piece = mv.Piece();
    PIECE captured = mv.Captured();
    PIECE promotion = mv.Promotion();

    switch (piece)
    {
        case PW: case PB: break;
        case NW: case NB: strPiece = "N"; break;
        case BW: case BB: strPiece = "B"; break;
        case RW: case RB: strPiece = "R"; break;
        case QW: case QB: strPiece = "Q"; break;
        case KW: case KB: strPiece = "K"; break;
        default: break;
    }

    strTo = FldToStr(to);

    if (captured)
    {
        strCapture = "x";
        if (piece == PW || piece == PB)
            strFrom = FldToStr(from).substr(0, 1);
    }

    switch (promotion)
    {
        case QW: case QB: strPromotion = "=Q"; break;
        case RW: case RB: strPromotion = "=R"; break;
        case BW: case BB: strPromotion = "=B"; break;
        case NW: case NB: strPromotion = "=N"; break;
        default: break;
    }

    bool ambiguity = false;
    bool uniqCol = true;
    bool uniqRow = true;

    auto mvSize = mvlist.Size();
    for (size_t i = 0; i < mvSize; ++i)
    {
        Move mv1 = mvlist[i].m_mv;
        if (mv1.From() == from)
            continue;
        if (mv1.To() != to)
            continue;
        if (mv1.Piece() != piece)
            continue;

        if (!pos.MakeMove(mv1))
            continue;
        pos.UnmakeMove();

        ambiguity = true;
        if (Col(mv1.From()) == Col(mv))
            uniqCol = false;
        if (Row(mv1.From()) == Row(mv))
            uniqRow = false;
    }

    if (ambiguity)
    {
        if (uniqCol)
            strFrom = FldToStr(from).substr(0, 1);
        else if (uniqRow)
            strFrom = FldToStr(from).substr(1, 1);
        else
            strFrom = FldToStr(from);
    }

    return strPiece + strFrom + strCapture + strTo + strPromotion;
}

