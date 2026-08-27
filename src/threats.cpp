/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2026 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

//
// Conceptual idea and network architecture design of threats is from Stockfish
//

#include "threats.h"
#include "position.h"

#include <array>
#include <utility>

namespace
{
    constexpr int colOf(int f)  { return f & 7;     }
    constexpr U64 single(int f) { return 1ull << f; }

    //
    // Target of a single step, empty when the step leaves the board or wraps a file
    //

    constexpr U64 step(int f, int delta) {
        const int to = f + delta;

        if (to < 0 || to > 63)
            return 0;

        const int dc = colOf(f) - colOf(to);
        return (dc <= 2 && dc >= -2) ? single(to) : 0ull;
    }

    constexpr U64 slide(int f, const int (&dirs)[4]) {
        U64 attacks = 0;

        for (int d : dirs)
            for (int cur = f; U64 next = step(cur, d); cur += d)
                attacks |= next;

        return attacks;
    }

    constexpr U64 hop(int f, const int (&steps)[8]) {
        U64 attacks = 0;

        for (int d : steps)
            attacks |= step(f, d);

        return attacks;
    }

    constexpr int countBits(U64 b) {
        int n = 0;

        for (; b; b &= b - 1)
            ++n;

        return n;
    }

    constexpr int getType(Piece p) { return int(p) & 7;  }
    constexpr int getSide(Piece p) { return int(p) >> 3; }

    constexpr int rookDirs[4]    = { 8, -8, 1, -1                       };
    constexpr int bishopDirs[4]  = { 9, 7, -7, -9                       };
    constexpr int knightSteps[8] = { 17, 15, 10, 6, -6, -10, -15, -17   };
    constexpr int kingSteps[8]   = { 8, -8, 1, -1, 9, 7, -7, -9         };

    constexpr Piece AllPieces[12] = {
        W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
        B_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING
    };

    constexpr int PAWN_TYPE = getType(W_PAWN);

    constexpr auto pseudoAttacks = []() {
        std::array<std::array<U64, 64>, 7> attacks{};

        for (int f = 0; f < 64; ++f) {
            attacks[0][f] = step(f, 7)  | step(f, 9);
            attacks[1][f] = step(f, -7) | step(f, -9);
            attacks[2][f] = hop(f, knightSteps);
            attacks[3][f] = slide(f, bishopDirs);
            attacks[4][f] = slide(f, rookDirs);
            attacks[5][f] = attacks[3][f] | attacks[4][f];
            attacks[6][f] = hop(f, kingSteps);
        }

        return attacks;
    }();

    // Mirror horizontally when the king stands on the king side
    constexpr auto orient = []() {
        std::array<std::uint8_t, 64> orient{};

        for (int f = 0; f < 64; ++f)
            orient[f] = colOf(f) >= 4 ? 7 : 0;

        return orient;
    }();

    constexpr int NumValidTargets[16] = { 0, 6, 10, 8, 8, 10, 0, 0, 0, 6, 10, 8, 8, 10, 0, 0 };

    // [attacker type][attacked type] to the class of the attacked piece, -1 when the pair is not encoded
    constexpr int TargetClass[6][6] =
    {
        {  0,  1, -1,  2, -1, -1 },
        {  0,  1,  2,  3,  4, -1 },
        {  0,  1,  2,  3, -1, -1 },
        {  0,  1,  2,  3, -1, -1 },
        {  0,  1,  2,  3,  4, -1 },
        { -1, -1, -1, -1, -1, -1 }
    };

    struct PieceBlock {
        int attackCount; // number of (from, to) pseudo attack pairs of this piece
        int firstIndex;  // first feature index belonging to this piece
    };

    constexpr auto pieceBlockTables = []() {
        std::array<PieceBlock, 16>                    blocks{};
        std::array<std::array<std::uint16_t, 64>, 16> squareOffsets{};

        int firstIndex = 0;

        for (Piece piece : AllPieces) {
            const int idx = int(piece);
            int attackCount = 0;

            for (int from = 0; from < 64; ++from) {
                squareOffsets[idx][from] = static_cast<std::uint16_t>(attackCount);

                if (getType(piece) != PAWN_TYPE)
                    attackCount += countBits(pseudoAttacks[getType(piece)][from]);
                else if (from >= 8 && from <= 55) // pawns never stand on the first and the last rank
                    attackCount += countBits(pseudoAttacks[getSide(piece)][from]);
            }

            blocks[idx] = { attackCount, firstIndex };
            firstIndex += NumValidTargets[idx] * attackCount;
        }

        return std::pair { blocks, squareOffsets };
    }();

    constexpr auto PieceBlocks   = pieceBlockTables.first;
    constexpr auto SquareOffsets = pieceBlockTables.second;

    static_assert(PieceBlocks[B_KING].firstIndex == int(THREAT_DIMENSIONS), "threat feature blocks do not add up to the feature set size");

    // [attacker][attacked][attacker square below attacked square] to the start of the pair block
    constexpr auto pairOffsets = []() {
        std::array<std::array<std::array<std::uint32_t, 2>, 16>, 16> out{};

        for (auto & attackerRow : out)
            for (auto & entry : attackerRow)
                entry = { THREAT_DIMENSIONS, THREAT_DIMENSIONS };

        for (Piece attacker : AllPieces) {
            for (Piece attacked : AllPieces) {
                const int attackerType = getType(attacker);
                const int attackedType = getType(attacked);
                const int targetClass  = TargetClass[attackerType - 1][attackedType - 1];

                if (targetClass < 0)
                    continue;

                const int feature = PieceBlocks[attacker].firstIndex + (getSide(attacked) * (NumValidTargets[attacker] / 2) + targetClass) * PieceBlocks[attacker].attackCount;

                //
                // Two pieces of the same type threaten each other symmetrically, so only
                // the ordering with the attacker on the higher square carries a feature.
                //

                const bool enemy     = (int(attacker) ^ int(attacked)) == 8;
                const bool symmetric = attackerType == attackedType && (enemy || attackerType != PAWN_TYPE);

                out[attacker][attacked][0] = static_cast<std::uint32_t>(feature);
                out[attacker][attacked][1] = symmetric ? THREAT_DIMENSIONS : static_cast<std::uint32_t>(feature);
            }
        }

        return out;
    }();

    //
    // [attacker][from] to the attack set of that piece on an empty board. The rank of a
    // target inside the set is the number of set bits below it, which is a popcount away
    // - a table holding the rank of every (from, to) pair instead would be eight times
    // this one and would not stay in the first level cache.
    //

    constexpr auto attackSet = []() {
        std::array<std::array<U64, 64>, 16> out{};

        for (Piece piece : AllPieces) {
            const int attacks = getType(piece) == PAWN_TYPE ? getSide(piece) : getType(piece);

            for (int from = 0; from < 64; ++from)
                out[int(piece)][from] = pseudoAttacks[attacks][from];
        }

        return out;
    }();

    //
    // How a point of view turns the board around, which is all a side contributes to an
    // index and is therefore worked out once per walk rather than once per threat.
    //
    // The board numbers A8 as 0, the feature layout numbers A1 as 0, so every square is
    // flipped vertically first. From the black point of view it is flipped once more,
    // which cancels out, and the colour of both pieces is swapped.
    //

    struct Perspective {
        unsigned squares; // turns a square around
        unsigned pieces;  // turns a piece code around
    };

    inline Perspective getPerspective(COLOR side, Square ksq) {
        return { unsigned(SQ_A8 ^ orient[ksq ^ SQ_A8] ^ (SQ_A8 * side)), 8u * side };
    }

    //
    // The part of an index that is settled once the attacker and the square it stands on
    // are known. A piece attacking several others pays for it once instead of per target.
    //

    struct Origin {
        const std::array<std::uint32_t, 2> * pairs;   // pair block starts, by attacked piece
        std::uint32_t                        square;  // start of this square inside the piece block
        U64                                  attacks; // where the attacker bears from here, on an empty board
        unsigned                             from;    // the origin square, turned around
    };

    inline Origin getOrigin(const Perspective & view, Piece attacker, Square from) {
        const unsigned attackerOriented = unsigned(attacker) ^ view.pieces;
        const unsigned fromOriented     = unsigned(from) ^ view.squares;

        return { pairOffsets[attackerOriented].data(), SquareOffsets[attackerOriented][fromOriented], attackSet[attackerOriented][fromOriented], fromOriented };
    }

    inline std::uint32_t getIndex(const Perspective & view, const Origin & origin, Square to, Piece attacked) {
        const unsigned toOriented = unsigned(to) ^ view.squares;
        const U64      below      = (1ull << toOriented) - 1;

        return origin.pairs[unsigned(attacked) ^ view.pieces][origin.from < toOriented] + origin.square + countBits(origin.attacks & below);
    }
}

std::uint32_t makeThreatIndex(COLOR side, Piece attacker, Square from, Square to, Piece attacked, Square ksq) {
    const Perspective view = getPerspective(side, ksq);

    return getIndex(view, getOrigin(view, attacker, from), to, attacked);
}

void getActiveThreatIndexes(const Position & pos, COLOR side, ThreatList & active) {
    const Square ksq      = pos.King(side);
    const U64    occupied = pos.BitsAll();

    const Perspective view = getPerspective(side, ksq);

    const U64 pawnTargets        = pos.Bits(PW) | pos.Bits(PB) | pos.Bits(NW) | pos.Bits(NB) | pos.Bits(RW) | pos.Bits(RB);
    const U64 minorSliderTargets = pawnTargets | pos.Bits(BW) | pos.Bits(BB);
    const U64 queenTargets       = minorSliderTargets | pos.Bits(QW) | pos.Bits(QB);

    for (COLOR i = 0; i < 2; ++i) {
        const COLOR attackerSide = side ^ i;

        for (PIECE type = PAWN; type < KING; type += 2) {

            const PIECE p        = static_cast<PIECE>(type | attackerSide);
            const Piece attacker = PieceAdapter[p];
            const U64   targets  = (type == PAWN)                    ? pawnTargets
                                 : (type == KNIGHT || type == QUEEN) ? queenTargets
                                                                     : minorSliderTargets;
            U64 bb = pos.Bits(p);

            while (bb) {

                const FLD from = PopLSB(bb);
                U64 attacks = Attacks(from, occupied, p) & targets;

                if (!attacks)
                    continue;

                const Origin origin = getOrigin(view, attacker, from);

                while (attacks) {
                    const FLD to = PopLSB(attacks);
                    active.add(getIndex(view, origin, to, PieceAdapter[pos[to]]));
                }
            }
        }
    }
}

void getChangedThreatIndexes(COLOR side, Square ksq, const DirtyThreats & dirty, ThreatList & removed, ThreatList & added) {
    const Perspective view = getPerspective(side, ksq);

    for (size_t i = 0; i < dirty.size(); ++i) {
        const auto & dt = dirty[i];
        const auto index = getIndex(view, getOrigin(view, dt.attacker(), dt.from()), dt.to(), dt.attacked());
        (dt.added() ? added : removed).add(index);
    }
}
