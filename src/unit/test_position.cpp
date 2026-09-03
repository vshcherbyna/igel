/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2018-2026 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
*
*  Igel is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*/

#include "../bitboards.h"
#include "../moves.h"
#include "../position.h"
#include "../search.h"
#include <gtest/gtest.h>
#include <memory>

namespace unit
{

namespace
{

bool insufficientMaterialFromFen(const char* fen)
{
    auto pos = std::make_unique<Position>();
    EXPECT_TRUE(pos->SetFEN(fen));
    return pos->isInsufficientMaterial();
}

}

TEST(InsufficientMaterial, DeadPositions)
{
    InitBitboards();
    Position::InitHashNumbers();

    EXPECT_TRUE(insufficientMaterialFromFen("7k/8/8/8/8/8/8/K7 w - - 0 1"));
    EXPECT_TRUE(insufficientMaterialFromFen("7k/8/8/8/2B5/8/8/K7 w - - 0 1"));
    EXPECT_TRUE(insufficientMaterialFromFen("7k/8/8/8/3N4/8/8/K7 w - - 0 1"));
    EXPECT_TRUE(insufficientMaterialFromFen("7k/8/2b5/8/8/5B2/8/K7 w - - 0 1"));
}

TEST(InsufficientMaterial, PotentialMatingMaterial)
{
    InitBitboards();
    Position::InitHashNumbers();

    EXPECT_FALSE(insufficientMaterialFromFen("7k/8/8/8/3NN3/8/8/K7 w - - 0 1"));
    EXPECT_FALSE(insufficientMaterialFromFen("7k/8/8/8/3BN3/8/8/K7 w - - 0 1"));
    EXPECT_FALSE(insufficientMaterialFromFen("6nk/8/8/8/3N4/8/8/K7 w - - 0 1"));
    EXPECT_FALSE(insufficientMaterialFromFen("6nk/8/8/8/2B5/8/8/K7 w - - 0 1"));
    EXPECT_FALSE(insufficientMaterialFromFen("7k/8/1b6/8/8/5B2/8/K7 w - - 0 1"));
    EXPECT_FALSE(insufficientMaterialFromFen("7k/8/8/8/3P4/8/8/K7 w - - 0 1"));
}

TEST(GameOver, MatePrecedesMaterialDraw)
{
    InitBitboards();
    Position::InitHashNumbers();

    auto pos = std::make_unique<Position>();
    ASSERT_TRUE(pos->SetFEN("8/8/8/8/8/8/nBK5/k7 b - - 0 1"));
    auto search = std::make_unique<Search>();
    std::string result;
    std::string comment;
    Move bestMove{};
    int legalMoves = 0;

    EXPECT_TRUE(search->isGameOver(*pos, result, comment, bestMove, legalMoves));
    EXPECT_EQ(0, legalMoves);
    EXPECT_EQ("1-0", result);
    EXPECT_EQ("{White mates}", comment);
}

}
