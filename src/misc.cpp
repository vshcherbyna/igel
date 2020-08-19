/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2020 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#if defined(EVAL_NNUE)

#include <malloc.h>
#include <cstdlib>

void* std_aligned_alloc(size_t alignment, size_t size) {
#if defined(__APPLE__)
    return aligned_alloc(alignment, size);
#elif defined(_WIN32)
    return _mm_malloc(size, alignment);
#else
    return std::aligned_alloc(alignment, size);
#endif
}

void std_aligned_free(void* ptr) {
#if defined(__APPLE__)
    free(ptr);
#elif defined(_WIN32)
    _mm_free(ptr);
#else
    free(ptr);
#endif
}
#endif
