/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2002-2018 Vladimir Medvedev <vrm@bk.ru> (GreKo author)
*  Copyright (C) 2018-2021 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

#if defined(EVAL_NNUE)
#include "incbin/incbin.h"

#if !defined(_MSC_VER)
INCBIN(EmbeddedNNUE, EVALFILE);
#endif // _MSC_VER

#include <streambuf>
using namespace std;
#endif // EVAL_NNUE

#if !defined(UNIT_TEST)
int main(int argc, const char* argv[])
{
    //
    //  initialize igel
    //

    InitBitboards();
    Position::InitHashNumbers();
    Evaluator::initEval();

#if defined(EVAL_NNUE)

    //
    //  initialize nnue
    //

    Eval::init_NNUE();

#if defined(_MSC_VER)

    //
    //  for debugging purposes load official network file ign-0-9b1937cc
    //

    if (!Eval::NNUE::load_eval_file("ign-1-139b702b")) {
        std::cout << "Unable to set EvalFile. Aborting" << std::endl;
        abort();
    }
#else

    //
    //  load network file from resources
    //

    class MemoryBuffer : public basic_streambuf<char> {
    public: MemoryBuffer(char* p, size_t n) { setg(p, p, p + n); setp(p, p + n); }
    };

    MemoryBuffer buffer(const_cast<char*>(reinterpret_cast<const char*>(gEmbeddedNNUEData)),
        size_t(gEmbeddedNNUESize));

    istream stream(&buffer);

    if (!Eval::NNUE::load_eval(EVALFILE, stream)) {
        std::cout << "Unable to set EvalFile. Aborting" << std::endl;
        abort();
    }

#endif // _MSC_VER
#endif // EVAL_NNUE

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
