/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2002-2018 Vladimir Medvedev <vrm@bk.ru> (GreKo author)
*  Copyright (C) 2018-2023 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

#include "evaluate.h"
#include "uci.h"

#if !defined(UNIT_TEST)
int main(int argc, const char* argv[])
{
    static_assert(USE_AVX2 == 1, "AVX2 is the minimum supported build type");

    //
    //  initialize igel
    //

    InitBitboards();
    Position::InitHashNumbers();
    Evaluator::initEval();

    std::unique_ptr<Search> searcher(new Search);

    searcher->m_position.SetInitial();
    searcher->setSyzygyDepth(1);

    //
    //  start uci communication handler
    //

    Uci handler(*searcher.get());

    if (argc == 2 && !strcmp(argv[1], "bench"))
        return handler.onBench();
    else
        return handler.handleCommands();
}
#endif
