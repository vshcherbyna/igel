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

#include "bitboards.h"
#include "utils.h"

// _BTYPE is set to 1 for BMI2 builds
// include headers for pext intrinsic
#if defined(_BTYPE) && (_BTYPE == 1)
#if _WIN32 || _WIN64
#include <immintrin.h>
#endif
#if __GNUC__
#include <x86intrin.h>
#endif
#endif

U64 BB_SINGLE[64];
U64 BB_DIR[64][8];
U64 BB_BETWEEN[64][64];

U64 BB_PAWN_ATTACKS[64][2];
U64 BB_KNIGHT_ATTACKS[64];
U64 BB_BISHOP_ATTACKS[64];
U64 BB_ROOK_ATTACKS[64];
U64 BB_QUEEN_ATTACKS[64];
U64 BB_KING_ATTACKS[64];

U64 BB_HORIZONTAL[8] =
{
    LL(0xff00000000000000),
    LL(0x00ff000000000000),
    LL(0x0000ff0000000000),
    LL(0x000000ff00000000),
    LL(0x00000000ff000000),
    LL(0x0000000000ff0000),
    LL(0x000000000000ff00),
    LL(0x00000000000000ff)
};

U64 BB_VERTICAL[8] =
{
    LL(0x8080808080808080),
    LL(0x4040404040404040),
    LL(0x2020202020202020),
    LL(0x1010101010101010),
    LL(0x0808080808080808),
    LL(0x0404040404040404),
    LL(0x0202020202020202),
    LL(0x0101010101010101)
};

U64 BB_FIRST_HORIZONTAL[2]   = { BB_HORIZONTAL[7], BB_HORIZONTAL[0] };
U64 BB_SECOND_HORIZONTAL[2]  = { BB_HORIZONTAL[6], BB_HORIZONTAL[1] };
U64 BB_THIRD_HORIZONTAL[2]   = { BB_HORIZONTAL[5], BB_HORIZONTAL[2] };
U64 BB_FOURTH_HORIZONTAL[2]  = { BB_HORIZONTAL[4], BB_HORIZONTAL[3] };
U64 BB_FIFTH_HORIZONTAL[2]   = { BB_HORIZONTAL[3], BB_HORIZONTAL[4] };
U64 BB_SIXTH_HORIZONTAL[2]   = { BB_HORIZONTAL[2], BB_HORIZONTAL[5] };
U64 BB_SEVENTH_HORIZONTAL[2] = { BB_HORIZONTAL[1], BB_HORIZONTAL[6] };
U64 BB_EIGHTH_HORIZONTAL[2]  = { BB_HORIZONTAL[0], BB_HORIZONTAL[7] };

U64 BB_PASSED_PAWN_MASK_SIDE[64][2];
U64 BB_PASSED_PAWN_MASK_OPP[64][2];
U64 BB_DOUBLED_PAWN_MASK[64][2];
U64 BB_ISOLATED_PAWN_MASK[64];
U64 BB_STRONG_FIELD_MASK[64][2];

U64 BB_PAWN_SQUARE[64][2];
U64 BB_PAWN_CONNECTED[64];

static const U64 B_MASK[64] =
{
    LL(0x0040201008040200), LL(0x0020100804020000), LL(0x0050080402000000), LL(0x0028440200000000),
    LL(0x0014224000000000), LL(0x000a102040000000), LL(0x0004081020400000), LL(0x0002040810204000),
    LL(0x0000402010080400), LL(0x0000201008040200), LL(0x0000500804020000), LL(0x0000284402000000),
    LL(0x0000142240000000), LL(0x00000a1020400000), LL(0x0000040810204000), LL(0x0000020408102000),
    LL(0x0040004020100800), LL(0x0020002010080400), LL(0x0050005008040200), LL(0x0028002844020000),
    LL(0x0014001422400000), LL(0x000a000a10204000), LL(0x0004000408102000), LL(0x0002000204081000),
    LL(0x0020400040201000), LL(0x0010200020100800), LL(0x0008500050080400), LL(0x0044280028440200),
    LL(0x0022140014224000), LL(0x00100a000a102000), LL(0x0008040004081000), LL(0x0004020002040800),
    LL(0x0010204000402000), LL(0x0008102000201000), LL(0x0004085000500800), LL(0x0002442800284400),
    LL(0x0040221400142200), LL(0x0020100a000a1000), LL(0x0010080400040800), LL(0x0008040200020400),
    LL(0x0008102040004000), LL(0x0004081020002000), LL(0x0002040850005000), LL(0x0000024428002800),
    LL(0x0000402214001400), LL(0x004020100a000a00), LL(0x0020100804000400), LL(0x0010080402000200),
    LL(0x0004081020400000), LL(0x0002040810200000), LL(0x0000020408500000), LL(0x0000000244280000),
    LL(0x0000004022140000), LL(0x00004020100a0000), LL(0x0040201008040000), LL(0x0020100804020000),
    LL(0x0002040810204000), LL(0x0000020408102000), LL(0x0000000204085000), LL(0x0000000002442800),
    LL(0x0000000040221400), LL(0x0000004020100a00), LL(0x0000402010080400), LL(0x0040201008040200)
};

static const int B_BITS[64] =
{
     6,  5,  5,  5,  5,  5,  5,  6,
     5,  5,  5,  5,  5,  5,  5,  5,
     5,  5,  7,  7,  7,  7,  5,  5,
     5,  5,  7,  9,  9,  7,  5,  5,
     5,  5,  7,  9,  9,  7,  5,  5,
     5,  5,  7,  7,  7,  7,  5,  5,
     5,  5,  5,  5,  5,  5,  5,  5,
     6,  5,  5,  5,  5,  5,  5,  6
};

#if defined(_BTYPE) && (_BTYPE == 1)
#else
static const int B_SHIFT[64] =
{
    58, 59, 59, 59, 59, 59, 59, 58,
    59, 59, 59, 59, 59, 59, 59, 59,
    59, 59, 57, 57, 57, 57, 59, 59,
    59, 59, 57, 55, 55, 57, 59, 59,
    59, 59, 57, 55, 55, 57, 59, 59,
    59, 59, 57, 57, 57, 57, 59, 59,
    59, 59, 59, 59, 59, 59, 59, 59,
    58, 59, 59, 59, 59, 59, 59, 58
};

static const U64 B_MULT[64] =
{
    LL(0x0040010202020020), LL(0x0800080801080200), LL(0x4000000802080600), LL(0x1040010010020200),
    LL(0x0800000400841400), LL(0x0081000021080800), LL(0x000000208404a000), LL(0x0001010100a00400),
    LL(0x0008020802006000), LL(0x0020021022008000), LL(0x0040401002048000), LL(0x0004000420820100),
    LL(0x020400020a020000), LL(0x1800002108080000), LL(0x0000804410044000), LL(0x0002011012100000),
    LL(0x8001010401000080), LL(0x0002040800800201), LL(0x0001010101004200), LL(0x004002020a000400),
    LL(0x0001204200800800), LL(0x0402020022000400), LL(0x0004030110000800), LL(0x2008040220000800),
    LL(0x0008004880010080), LL(0x0010040840008200), LL(0x0002020201004800), LL(0x00040100100400c0),
    LL(0x00c0020080080080), LL(0x0002010440100040), LL(0x0804100200040400), LL(0x8008201000080200),
    LL(0x0002028000208800), LL(0x0001040000420802), LL(0x0042020000480200), LL(0x0002840000822000),
    LL(0x0021004004040002), LL(0x0002440008002400), LL(0x4002020020040400), LL(0x0002200008208400),
    LL(0x0000400205008800), LL(0x0002000108010c00), LL(0x0000200410080810), LL(0x0001008090400100),
    LL(0x0048000082004100), LL(0x0010001800202020), LL(0x0003001010020080), LL(0x1004001004100400),
    LL(0x0000082082101000), LL(0x0000010090104800), LL(0x0000011120104000), LL(0x0010111040080000),
    LL(0x4040044400800000), LL(0x0000040404004102), LL(0x0000600204004080), LL(0x0000040806080200),
    LL(0x0000802101104000), LL(0x0001042120080000), LL(0x0001042084100000), LL(0x0001104044000020),
    LL(0x00080a0020004004), LL(0x0104041400400000), LL(0x0020050200810000), LL(0x0020040102002200)
};

static const int R_SHIFT[64] =
{
    52, 53, 53, 53, 53, 53, 53, 52,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    52, 53, 53, 53, 53, 53, 53, 52
};

static const U64 R_MULT[64] =
{
    LL(0x8000040020408102), LL(0x0000100088030204), LL(0x0041000400020881), LL(0x0002000810210402),
    LL(0x00010008a0100005), LL(0x0000081040208202), LL(0x0000208411004001), LL(0x0002008100104022),
    LL(0x0000048500440200), LL(0x0081001200040300), LL(0x1000800200140180), LL(0x0010110500080100),
    LL(0x1040880010008080), LL(0x1001110040200100), LL(0x0400200080400580), LL(0x0100800040016080),
    LL(0x0000018420420001), LL(0x000001100884000a), LL(0x0024008006008004), LL(0x8048008004008008),
    LL(0x020040100a020020), LL(0x021000240800a000), LL(0x8000a01000484000), LL(0x0004208040048010),
    LL(0x0000010046000284), LL(0x0002009102000408), LL(0x0005004803000400), LL(0x0001800800805400),
    LL(0x1024100080800800), LL(0x0030809000802000), LL(0x0840100800200060), LL(0x0500814000800020),
    LL(0x0000010200208444), LL(0x0010010400081082), LL(0x8000120080440080), LL(0x2401001100080004),
    LL(0x2000210100281000), LL(0x0003100080200084), LL(0x0014401080200080), LL(0x001080208000400c),
    LL(0x0000120000428104), LL(0x0042040010080201), LL(0x0000808064004200), LL(0x0814010100080010),
    LL(0x0002020008304020), LL(0x1080808020005000), LL(0x000040c000e01000), LL(0x088080800020c000),
    LL(0x4200800080044100), LL(0x000300020011000c), LL(0x0042000200281004), LL(0x1280800800040080),
    LL(0x4000805000080280), LL(0x4040802000805000), LL(0x0001004000208109), LL(0x0000801084400120),
    LL(0x1080050000402080), LL(0x0c80220001000080), LL(0x0500280100040002), LL(0x0280120400800800),
    LL(0x0080041000480082), LL(0x0100090040122000), LL(0x0040011000442004), LL(0x0280008040002110)
};

#endif

static const U64 R_MASK[64] =
{
    LL(0x7e80808080808000), LL(0x3e40404040404000), LL(0x5e20202020202000), LL(0x6e10101010101000),
    LL(0x7608080808080800), LL(0x7a04040404040400), LL(0x7c02020202020200), LL(0x7e01010101010100),
    LL(0x007e808080808000), LL(0x003e404040404000), LL(0x005e202020202000), LL(0x006e101010101000),
    LL(0x0076080808080800), LL(0x007a040404040400), LL(0x007c020202020200), LL(0x007e010101010100),
    LL(0x00807e8080808000), LL(0x00403e4040404000), LL(0x00205e2020202000), LL(0x00106e1010101000),
    LL(0x0008760808080800), LL(0x00047a0404040400), LL(0x00027c0202020200), LL(0x00017e0101010100),
    LL(0x0080807e80808000), LL(0x0040403e40404000), LL(0x0020205e20202000), LL(0x0010106e10101000),
    LL(0x0008087608080800), LL(0x0004047a04040400), LL(0x0002027c02020200), LL(0x0001017e01010100),
    LL(0x008080807e808000), LL(0x004040403e404000), LL(0x002020205e202000), LL(0x001010106e101000),
    LL(0x0008080876080800), LL(0x000404047a040400), LL(0x000202027c020200), LL(0x000101017e010100),
    LL(0x00808080807e8000), LL(0x00404040403e4000), LL(0x00202020205e2000), LL(0x00101010106e1000),
    LL(0x0008080808760800), LL(0x00040404047a0400), LL(0x00020202027c0200), LL(0x00010101017e0100),
    LL(0x0080808080807e00), LL(0x0040404040403e00), LL(0x0020202020205e00), LL(0x0010101010106e00),
    LL(0x0008080808087600), LL(0x0004040404047a00), LL(0x0002020202027c00), LL(0x0001010101017e00),
    LL(0x008080808080807e), LL(0x004040404040403e), LL(0x002020202020205e), LL(0x001010101010106e),
    LL(0x0008080808080876), LL(0x000404040404047a), LL(0x000202020202027c), LL(0x000101010101017e)
};

static const int R_BITS[64] =
{
    12, 11, 11, 11, 11, 11, 11, 12,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    12, 11, 11, 11, 11, 11, 11, 12
};

static int B_OFFSET[64];
static int R_OFFSET[64];

U64 * B_DATA = nullptr;
U64 * R_DATA = nullptr;

U64 Attacks(FLD f, U64 occ, PIECE piece)
{
    switch (piece)
    {
        case PW:
            return BB_PAWN_ATTACKS[f][WHITE];
        case PB:
            return BB_PAWN_ATTACKS[f][BLACK];
        case NW:
        case NB:
            return BB_KNIGHT_ATTACKS[f];
        case BW:
        case BB:
            return BishopAttacks(f, occ);
        case RW:
        case RB:
            return RookAttacks(f, occ);
        case QW:
        case QB:
            return QueenAttacks(f, occ);
        case KW:
        case KB:
            return BB_KING_ATTACKS[f];
        default:
            return 0;
    }
}

U64 BishopAttacks(FLD f, U64 occ)
{
#if defined(_BTYPE) && (_BTYPE == 1)
    int index = _pext_u64(occ, B_MASK[f]);
#else
    int index = int(((occ & B_MASK[f]) * B_MULT[f]) >> B_SHIFT[f]);
#endif
    return B_DATA[B_OFFSET[f] + index];
}

U64 BishopAttacksTrace(FLD f, U64 occ)
{
    U64 att = 0;
    for (int dir = 1; dir < 8; dir += 2)
    {
        U64 x = Shift(BB_SINGLE[f], dir);
        while (x)
        {
            att |= x;
            if (x & occ)
                break;
            x = Shift(x, dir);
        }
    }
    return att;
}

int Delta(int dir)
{
    assert(dir >= 0 && dir <= 7);

    static int delta[8] = { 1, -7, -8, -9, -1, 7, 8, 9 };
    return delta[dir];
}

U64 EnumBits(U64 b, int n)
{
    U64 r = 0;
    while (b && n)
    {
        r |= (n & 1) * BB_SINGLE[PopLSB(b)];
        n >>= 1;
    }
    return r;
}

void FindMagicLSB()
{
    U32 inputs[64], outputs[64], mult = 0, N = 0, best = 0;
    FLD f;

    for (f = 0; f < 64; ++f)
    {
        U64 x = BB_SINGLE[f];
        x ^= (x - 1);
        inputs[f] = U32(x) ^ U32(x >> 32);
    }

    RandSeed(30147);
    while (1)
    {
        mult = Rand32();
        memset(outputs, 0, 64 * sizeof(outputs[0]));

        for (f = 0; f < 64; ++f)
        {
            int index = (inputs[f] * mult) >> (32 - 6);
            if (outputs[index])
                break;
            outputs[index] = f + 1;
        }

        if (f > best)
            best = f;
        if (f == 64)
            break;
        if ((N % 1000000) == 0)
            std::cout << N / 1000000 << " M, best = " << best << "\r";
        ++N;
    }

    std::cout << N << " M, best = " << best << std::endl;
    std::cout << "mult = 0x" << std::hex << mult << std::dec << std::endl << std::endl;
    for (f = 0; f < 64; ++f)
    {
        std::cout << std::setw(2) << outputs[f] - 1;
        if (f < 63)
            std::cout << ", ";
        if (Col(f) == 7)
            std::cout << std::endl;
    }
}

void FindMaskB()
{
    U64 mask[64];
    for (FLD f = 0; f < 64; ++f)
    {
        mask[f] = BB_BISHOP_ATTACKS[f] & LL(0x007e7e7e7e7e7e00);
    }

    PrintArray(mask);
    std::cout << std::endl;

    for (FLD f = 0; f < 64; ++f)
    {
        std::cout << std::setw(2) << std::setfill(' ') << countBits(mask[f]);
        if (f < 63)
            std::cout << ", ";
        if (Col(f) == 7)
            std::cout << std::endl;
    }
}

void FindMaskR()
{
    U64 mask[64];
    for (FLD f = 0; f < 64; ++f)
    {
        mask[f] = BB_ROOK_ATTACKS[f];
        if (Col(f) != 0)
            mask[f] &= LL(0x7f7f7f7f7f7f7f7f);
        if (Col(f) != 7)
            mask[f] &= LL(0xfefefefefefefefe);
        if (Row(f) != 0)
            mask[f] &= LL(0x00ffffffffffffff);
        if (Row(f) != 7)
            mask[f] &= LL(0xffffffffffffff00);
    }

    PrintArray(mask);
    std::cout << std::endl;

    for (FLD f = 0; f < 64; ++f)
    {
        std::cout << std::setw(2) << std::setfill(' ') << countBits(mask[f]);
        if (f < 63)
            std::cout << ", ";
        if (Col(f) == 7)
            std::cout << std::endl;
    }
}

U64 FindMultB(FLD f)
{
    U64 mask = B_MASK[f];
    int bits = B_BITS[f];
    int N = 1 << bits;

    U64 mult;
    int n;

    std::vector<U64> inputs;
    inputs.resize(N);

    for (n = 0; n < N; ++n)
        inputs[n] = EnumBits(mask, n);

    while (1)
    {
        mult = Rand64(6);

        std::vector<U8> outputs;
        outputs.resize(N);

        for (n = 0; n < N; ++n)
        {
            int index = int((inputs[n] * mult) >> (64 - bits));
            if (outputs[index])
                break;
            outputs[index] = 1;
        }
        if (n == N)
            break;
    }

    return mult;
}

void FindMultB()
{
    U64 arr[64];
    memset(arr, 0, 64 * sizeof(arr[0]));

    for (FLD f = 0; f < 64; ++f)
    {
        arr[f] = FindMultB(f);
        PrintArray(arr);
    }
}

U64 FindMultR(FLD f)
{
    U64 mask = R_MASK[f];
    int bits = R_BITS[f];
    int N = 1 << bits;

    U64 mult;
    int n;

    std::vector<U64> inputs;
    inputs.resize(N);

    for (n = 0; n < N; ++n)
        inputs[n] = EnumBits(mask, n);

    while (1)
    {
        mult = Rand64(7);

        std::vector<U8> outputs;
        outputs.resize(N);

        for (n = 0; n < N; ++n)
        {
            int index = int((inputs[n] * mult) >> (64 - bits));
            if (outputs[index])
                break;
            outputs[index] = 1;
        }
        if (n == N)
            break;
    }

    return mult;
}

void FindMultR()
{
    U64 arr[64];
    memset(arr, 0, 64 * sizeof(arr[0]));

    for (FLD f = 0; f < 64; ++f)
    {
        arr[f] = FindMultR(f);
        PrintArray(arr);
    }
}

void FindShiftB()
{
    for (FLD f = 0; f < 64; ++f)
    {
        std::cout << std::setw(2) << 64 - B_BITS[f];
        if (f < 63)
            std::cout << ", ";
        if (Col(f) == 7)
            std::cout << std::endl;
    }
}

void FindShiftR()
{
    for (FLD f = 0; f < 64; ++f)
    {
        std::cout << std::setw(2) << 64 - R_BITS[f];
        if (f < 63)
            std::cout << ", ";
        if (Col(f) == 7)
            std::cout << std::endl;
    }
}

void InitBitboards()
{
    U64 x,y;
    FLD f, from, to;
    int delta;

    x = LL(0x8000000000000000);
    for (f = 0; f < 64; ++f)
    {
        BB_SINGLE[f] = x;
        x >>= 1;
    }

    for (from = 0; from < 64; ++from)
    {
        for (to = 0; to < 64; ++to)
            BB_BETWEEN[from][to] = 0;

        for (int dir = 0; dir < 8; ++dir)
        {
            x = Shift(BB_SINGLE[from], dir);
            y = 0;
            delta = Delta(dir);
            to = from + delta;
            while (x)
            {
                BB_BETWEEN[from][to] = y;
                y |= x;
                x = Shift(x, dir);
                to += delta;
            }
            BB_DIR[from][dir] = y;
        }
        
        BB_BISHOP_ATTACKS[from] =
            BB_DIR[from][DIR_UR] |
            BB_DIR[from][DIR_UL] |
            BB_DIR[from][DIR_DL] |
            BB_DIR[from][DIR_DR];

        BB_ROOK_ATTACKS[from] =
            BB_DIR[from][DIR_R] |
            BB_DIR[from][DIR_U] |
            BB_DIR[from][DIR_L] |
            BB_DIR[from][DIR_D];

        BB_QUEEN_ATTACKS[from] =
            BB_BISHOP_ATTACKS[from] |
            BB_ROOK_ATTACKS[from];

        x = BB_SINGLE[from];

        y = 0;
        y |= Right(UpRight(x));
        y |= Up(UpRight(x));
        y |= Up(UpLeft(x));
        y |= Left(UpLeft(x));
        y |= Left(DownLeft(x));
        y |= Down(DownLeft(x));
        y |= Down(DownRight(x));
        y |= Right(DownRight(x));
        BB_KNIGHT_ATTACKS[from] = y;

        y = 0;
        y |= Right(x);
        y |= UpRight(x);
        y |= Up(x);
        y |= UpLeft(x);
        y |= Left(x);
        y |= DownLeft(x);
        y |= Down(x);
        y |= DownRight(x);
        BB_KING_ATTACKS[from] = y;

        y = 0;
        y |= UpRight(x);
        y |= UpLeft(x);
        BB_PAWN_ATTACKS[from][WHITE] = y;

        y = 0;
        y |= DownRight(x);
        y |= DownLeft(x);
        BB_PAWN_ATTACKS[from][BLACK] = y;
    }

    for (f = 0; f < 64; ++f)
    {
        x = BB_DIR[f][DIR_U];
        BB_DOUBLED_PAWN_MASK[f][WHITE] = x;
        BB_PASSED_PAWN_MASK_SIDE[f][WHITE] = x;
        BB_PASSED_PAWN_MASK_OPP[f][WHITE] = x | Left(x) | Right(x);
        BB_STRONG_FIELD_MASK[f][WHITE] = Left(x) | Right(x);

        x = BB_DIR[f][DIR_D];
        BB_DOUBLED_PAWN_MASK[f][BLACK] = x;
        BB_PASSED_PAWN_MASK_SIDE[f][BLACK] = x;
        BB_PASSED_PAWN_MASK_OPP[f][BLACK] = x | Left(x) | Right(x);
        BB_STRONG_FIELD_MASK[f][BLACK] = Left(x) | Right(x);

        x = BB_VERTICAL[Col(f)];
        BB_ISOLATED_PAWN_MASK[f] = Left(x) | Right(x);
    }

    // pawn squares and connected pawns
    for (f = 0; f < 64; f++)
    {
        x = BB_DIR[f][DIR_U] | BB_SINGLE[f];
        for (int j = 0; j < Row(f); j++)
        {
            x |= Right(x);
            x |= Left(x);
        }
        BB_PAWN_SQUARE[f][WHITE] = x;

        x = BB_DIR[f][DIR_D] | BB_SINGLE[f];
        for (int j = 0; j < 7 - Row(f); j++)
        {
            x |= Right(x);
            x |= Left(x);
        }
        BB_PAWN_SQUARE[f][BLACK] = x;

        x = BB_SINGLE[f];
        x = Left(x) | Right(x);
        x |= Up(x);
        x |= Down(x);
        BB_PAWN_CONNECTED[f] = x;
    }

    // magic stuff

    int b_offset = 0, r_offset = 0;
    for (f = 0; f < 64; ++f)
    {
        B_OFFSET[f] = b_offset;
        R_OFFSET[f] = r_offset;
        b_offset += (1 << B_BITS[f]);
        r_offset += (1 << R_BITS[f]);
    }

    assert(B_DATA == nullptr);
    B_DATA = (U64*)malloc(b_offset * sizeof(U64));

    for (f = 0; f < 64; ++f)
    {
        U64 mask = B_MASK[f];
        int bits = B_BITS[f];
        int N = (1 << bits);

        for (int n = 0; n < N; ++n)
        {
            U64 occ = EnumBits(mask, n);
#if defined(_BTYPE) && (_BTYPE == 1)
            int index = _pext_u64(occ, mask);
#else
            U64 mult = B_MULT[f];
            int index = int((occ * mult) >> (64 - bits));
#endif
            B_DATA[B_OFFSET[f] + index] =  BishopAttacksTrace(f, occ);
        }
    }

    assert(R_DATA == nullptr);
    R_DATA = (U64*)malloc(r_offset * sizeof(U64));

    for (f = 0; f < 64; ++f)
    {
        U64 mask = R_MASK[f];
        int bits = R_BITS[f];
        int N = (1 << bits);

        for (int n = 0; n < N; ++n)
        {
            U64 occ = EnumBits(mask, n);
#if defined(_BTYPE) && (_BTYPE == 1)
            int index = _pext_u64(occ, mask);
#else
            U64 mult = R_MULT[f];
            int index = int((occ * mult) >> (64 - bits));
#endif
            R_DATA[R_OFFSET[f] + index] =  RookAttacksTrace(f, occ);
        }
    }
}

void Print(U64 b)
{
    std::cout << std::endl;
    for (FLD f = 0; f < 64; ++f)
    {
        if (b & BB_SINGLE[f])
            std::cout << " 1";
        else
            std::cout << " -";
        if (Col(f) == 7)
            std::cout << std::endl;
    }
    std::cout << std::endl;
}

void PrintArray(const U64* arr)
{
    std::cout << std::endl;
    for (FLD f = 0; f < 64; ++f)
    {
        PrintHex(arr[f]);
        if (f < 63)
            std::cout << ", ";
        if ((f % 4) == 3)
            std::cout << std::endl;
    }
    std::cout << std::endl;
}

void PrintHex(U64 b)
{
    std::cout << "LL(0x" << std::setw(16) << std::setfill('0') << std::hex << b << std::dec << ")";
}

U64 QueenAttacks(FLD f, U64 occ)
{
    return BishopAttacks(f, occ) | RookAttacks(f, occ);
}

U64 QueenAttacksTrace(FLD f, U64 occ)
{
    return BishopAttacksTrace(f, occ) | RookAttacksTrace(f, occ);
}

U64 RookAttacks(FLD f, U64 occ)
{
#if defined(_BTYPE) && (_BTYPE == 1)
    int index = _pext_u64(occ, R_MASK[f]);
#else
    int index = int(((occ & R_MASK[f]) * R_MULT[f]) >> R_SHIFT[f]);
#endif
    return R_DATA[R_OFFSET[f] + index];
}

U64 RookAttacksTrace(FLD f, U64 occ)
{
    U64 att = 0;
    for (int dir = 0; dir < 8; dir += 2)
    {
        U64 x = Shift(BB_SINGLE[f], dir);
        while (x)
        {
            att |= x;
            if (x & occ)
                break;
            x = Shift(x, dir);
        }
    }
    return att;
}

U64 Shift(U64 b, int dir)
{
    assert(dir >= 0 && dir <= 7);

    switch (dir)
    {
        case DIR_R:  return Right(b);
        case DIR_UR: return UpRight(b);
        case DIR_U:  return Up(b);
        case DIR_UL: return UpLeft(b);
        case DIR_L:  return Left(b);
        case DIR_DL: return DownLeft(b);
        case DIR_D:  return Down(b);
        case DIR_DR: return DownRight(b);
        default:     return 0;
    }
}

void TestMagic()
{
    U32 t0 = GetProcTime();
    RandSeed(time(0));
    for (int i = 0; i < 1000000; ++i)
    {
        for (FLD f = 0; f < 64; ++f)
        {
            U64 occ = Rand64();
            U64 att1 = QueenAttacksTrace(f, occ);
            U64 att2 = QueenAttacks(f, occ);
            if (att1 != att2)
            {
                Print(occ);
                Print(att1);
                Print(att2);
                std::cout << "ERROR - Test failed" << std::endl;
                return;
            }
            if (i % 1000 == 0)
                std::cout << i / 1000 << "...\r";
        }
    }
    U32 t1 = GetProcTime();
    std::cout << "OK - Test passed in " << (t1 - t0) / 1000. << " sec." << std::endl;
}

