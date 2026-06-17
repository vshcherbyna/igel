/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2023 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

#include "nnue.h"
#include "position.h"
#include "utils.h"
#include <immintrin.h>
#include <streambuf>

#include "incbin/incbin.h"

#if !defined(_MSC_VER)
INCBIN(EmbeddedNNUE, EVALFILE);
#endif // _MSC_VER

/*static */std::unique_ptr<Transformer> Evaluator::m_transformer;
/*static */std::vector<std::unique_ptr<LayeredNetwork>> Evaluator::m_networks;

EVAL Evaluator::evaluate(Position & pos)
{
    EVAL scale = 600 + 20 * pos.nonPawnMaterial() / 1024;
    EVAL v = static_cast<EVAL>(NnueEvaluate(pos) * scale / 1024);

    v = v * (208 - pos.Fifty()) / 208;

    return v + Tempo;
}

void Evaluator::initEval()
{
#if !defined(_MSC_VER)
    class MemoryBuffer : public std::basic_streambuf<char> {
        public: MemoryBuffer(char* p, size_t n) { setg(p, p, p + n); setp(p, p + n); }
    };

    MemoryBuffer buffer(const_cast<char*>(reinterpret_cast<const char*>(gEmbeddedNNUEData)), size_t(gEmbeddedNNUESize));
    std::istream stream(&buffer);
#else
    std::ifstream stream("network_file", std::ios::binary);
#endif

    initEval(stream);
}

/*static */void Evaluator::initEval(std::istream & stream) {

    std::uint32_t version, size, hash_value;
    stream.read(reinterpret_cast<char*>(&version), sizeof(version));
    stream.read(reinterpret_cast<char*>(&hash_value), sizeof(hash_value));
    stream.read(reinterpret_cast<char*>(&size), sizeof(size));
    std::string architecture;
    architecture.resize(size);
    stream.read(&(architecture)[0], size);

    m_transformer.reset(new Transformer(stream));
    m_networks.clear();

    for (auto i = 0; i < LAYERED_NETWORKS; ++i) {
        stream.read(reinterpret_cast<char*>(&version), sizeof(version));
        m_networks.emplace_back(new LayeredNetwork(stream));
    }

    assert(stream.peek() == std::ios::traits_type::eof());
}

int Evaluator::NnueEvaluate(Position & pos) {

    auto & accumulator = pos.state()->accumulator;

    //if (accumulator.computed_score)// - stronger on this net arch when disabled
    //    return accumulator.score;

    //
    // transform the features
    //

    const std::size_t bucket = (countBits(pos.BitsAll()) - 1) / 4;

    alignas(CACHE_LINE) std::uint8_t features[1024];
    auto psqt = m_transformer->transform(pos, features, bucket);

    //
    // call network evaluation
    //

    alignas(CACHE_LINE) char buffer[384];
    auto output = m_networks[bucket]->propagate(features, buffer);

    //
    // scale the result
    //

    int eval      = output[0];
    int delta     = 7;

    accumulator.score = static_cast<int>(((128 - delta) * psqt + (128 + delta) * eval) / 128 / WEIGHTS_SCALE);
    accumulator.computed_score = true;

    return accumulator.score;
}

Transformer::Transformer(std::istream & s) {
    std::uint32_t header;
    s.read(reinterpret_cast<char*>(&header), sizeof(header));
    s.read(reinterpret_cast<char*>(biases), sizeof(biases));
    s.read(reinterpret_cast<char*>(weights), sizeof(weights));
    s.read(reinterpret_cast<char*>(psqts), sizeof(psqts));
}

inline __m256i vec_msb_pack_16(__m256i a, __m256i b) {
    __m256i compacted = _mm256_packs_epi16(_mm256_srli_epi16(a, 7), _mm256_srli_epi16(b, 7));
    return _mm256_permute4x64_epi64(compacted, 0b11011000);
}

std::int32_t Transformer::transform(Position & pos, std::uint8_t * outBuffer, const std::size_t bucket) {

    const auto s = pos.state();

    if (!s->accumulator.computed_accumulation) {
        // Walk back through null-move states (dirty_num==0, not computed) to find a valid base.
        // Null moves don't move any pieces, so their accumulator == previous accumulator.
        const Undo* base = s->previous;
        while (base && !base->accumulator.computed_accumulation && base->dirtyPiece.dirty_num == 0)
            base = base->previous;

        if (base && base->accumulator.computed_accumulation)
            incremental(pos, (base != s->previous) ? &base->accumulator : nullptr);
        else
            refresh(pos);
        s->accumulator.computed_accumulation = true;
        s->accumulator.computed_score = false;
    }

    auto & acc  = s->accumulator.accumulation;
    auto & psqt = s->accumulator.psqtAccumulation;

    const Color sides[2] = { pos.Side(), !pos.Side() };
    const auto pt = (psqt[static_cast<int>(sides[0])][bucket] - psqt[static_cast<int>(sides[1])][bucket]) / 2;

    for (std::uint32_t side = 0; side < 2; ++side) {
        std::uint32_t offset = (HalfDimensions / 2) * side;

#if defined(USE_AVX2)
        constexpr std::uint32_t OutputChunkSize = MAX_CHUNK_SIZE;
        static_assert((HalfDimensions / 2) % OutputChunkSize == 0);
        constexpr std::uint32_t NumOutputChunks = HalfDimensions / 2 / OutputChunkSize;

        __m256i Zero = _mm256_setzero_si256();
        __m256i One = _mm256_set1_epi16(127);

        const __m256i* in0 = reinterpret_cast<const __m256i*>(&(acc[sides[side]][0]));
        const __m256i* in1 = reinterpret_cast<const __m256i*>(&(acc[sides[side]][HalfDimensions / 2]));
        __m256i* out = reinterpret_cast<__m256i*>(outBuffer + offset);

        for (std::uint32_t j = 0; j < NumOutputChunks; j += 1) {
            const __m256i sum0a = _mm256_max_epi16(_mm256_min_epi16(in0[j * 2 + 0], One), Zero);
            const __m256i sum0b = _mm256_max_epi16(_mm256_min_epi16(in0[j * 2 + 1], One), Zero);
            const __m256i sum1a = _mm256_max_epi16(_mm256_min_epi16(in1[j * 2 + 0], One), Zero);
            const __m256i sum1b = _mm256_max_epi16(_mm256_min_epi16(in1[j * 2 + 1], One), Zero);

            const __m256i pa = _mm256_mullo_epi16(sum0a, sum1a);
            const __m256i pb = _mm256_mullo_epi16(sum0b, sum1b);

            out[j] = vec_msb_pack_16(pa, pb);
        }
#endif
    }

    return pt;
}

inline void Transformer::incremental(Position & pos, const Accumulator * baseAcc) {
    const auto & prev_accumulator = baseAcc ? *baseAcc : pos.state()->previous->accumulator;
    auto & accumulator = pos.state()->accumulator;
    const auto & dp = pos.state()->dirtyPiece;

    alignas(CACHE_LINE) std::uint32_t added[32];
    alignas(CACHE_LINE) std::uint32_t removed[32];

    std::pair<std::uint32_t, std::uint32_t> pa;

    for (COLOR c : { WHITE, BLACK }) {
        auto fullUpdate = false;

        if (dp.dirty_num) { // a piece moved
            fullUpdate = dp.pieceId[0] == PIECE_ID_KING + c;
            if (fullUpdate)
                pa.first = pos.getActiveIndexes(c, added);
            else
                pa = pos.getChangedIndexes(c, added, removed);
        }

#if defined(USE_AVX2)
        std::uint32_t chunks = HalfDimensions / (SIMD_WIDTH / 2);
        auto accumulation = reinterpret_cast<__m256i*>(&accumulator.accumulation[c][0]);
#endif

        if (fullUpdate) {
            std::memcpy(accumulator.accumulation[c], biases, HalfDimensions * sizeof(std::int16_t));

#if defined(USE_AVX2)
            _mm256_store_si256(reinterpret_cast<__m256i*>(&accumulator.psqtAccumulation[c][0]), _mm256_setzero_si256());
#else
            for (std::size_t k = 0; k < PSQT_BUCKETS; ++k)
                accumulator.psqtAccumulation[c][k] = 0;
#endif

            for (std::uint32_t index = 0; index < pa.first; index++) {
                std::uint32_t offset = HalfDimensions * added[index];

#if defined(USE_AVX2)
                {
                    auto* p = reinterpret_cast<__m256i*>(&accumulator.psqtAccumulation[c][0]);
                    *p = _mm256_add_epi32(*p, _mm256_load_si256(reinterpret_cast<const __m256i*>(&psqts[added[index] * PSQT_BUCKETS])));
                }
#else
                for (std::size_t k = 0; k < PSQT_BUCKETS; ++k)
                    accumulator.psqtAccumulation[c][k] += psqts[added[index] * PSQT_BUCKETS + k];
#endif

#if defined(USE_AVX2)
                auto column = reinterpret_cast<const __m256i*>(&weights[offset]);
                for (std::uint32_t j = 0; j < chunks; ++j)
                    accumulation[j] = _mm256_add_epi16(accumulation[j], column[j]);
#endif
            }
        }
        else {
#if defined(USE_AVX2)
            // Fast path: fuse copy+sub+add into a single AVX2 pass (avoids re-reading accumulation)
            if (pa.second == 1 && pa.first == 1) {
                const auto prev_acc = reinterpret_cast<const __m256i*>(&prev_accumulator.accumulation[c][0]);
                const auto rem_col  = reinterpret_cast<const __m256i*>(&weights[HalfDimensions * removed[0]]);
                const auto add_col  = reinterpret_cast<const __m256i*>(&weights[HalfDimensions * added[0]]);
                for (std::uint32_t j = 0; j < chunks; ++j)
                    accumulation[j] = _mm256_add_epi16(_mm256_sub_epi16(prev_acc[j], rem_col[j]), add_col[j]);

                {
                    const __m256i prev_p = _mm256_load_si256(reinterpret_cast<const __m256i*>(&prev_accumulator.psqtAccumulation[c][0]));
                    const __m256i rem_p  = _mm256_load_si256(reinterpret_cast<const __m256i*>(&psqts[removed[0] * PSQT_BUCKETS]));
                    const __m256i add_p  = _mm256_load_si256(reinterpret_cast<const __m256i*>(&psqts[added[0]   * PSQT_BUCKETS]));
                    _mm256_store_si256(reinterpret_cast<__m256i*>(&accumulator.psqtAccumulation[c][0]),
                        _mm256_add_epi32(_mm256_sub_epi32(prev_p, rem_p), add_p));
                }
            }
            // Fast path for captures: fuse copy+sub+sub+add into a single AVX2 pass
            else if (pa.second == 2 && pa.first == 1) {
                const auto prev_acc = reinterpret_cast<const __m256i*>(&prev_accumulator.accumulation[c][0]);
                const auto rem0     = reinterpret_cast<const __m256i*>(&weights[HalfDimensions * removed[0]]);
                const auto rem1     = reinterpret_cast<const __m256i*>(&weights[HalfDimensions * removed[1]]);
                const auto add_col  = reinterpret_cast<const __m256i*>(&weights[HalfDimensions * added[0]]);
                for (std::uint32_t j = 0; j < chunks; ++j)
                    accumulation[j] = _mm256_add_epi16(
                        _mm256_sub_epi16(_mm256_sub_epi16(prev_acc[j], rem0[j]), rem1[j]),
                        add_col[j]);

                {
                    const __m256i prev_p = _mm256_load_si256(reinterpret_cast<const __m256i*>(&prev_accumulator.psqtAccumulation[c][0]));
                    const __m256i rem0_p = _mm256_load_si256(reinterpret_cast<const __m256i*>(&psqts[removed[0] * PSQT_BUCKETS]));
                    const __m256i rem1_p = _mm256_load_si256(reinterpret_cast<const __m256i*>(&psqts[removed[1] * PSQT_BUCKETS]));
                    const __m256i add_p  = _mm256_load_si256(reinterpret_cast<const __m256i*>(&psqts[added[0]   * PSQT_BUCKETS]));
                    _mm256_store_si256(reinterpret_cast<__m256i*>(&accumulator.psqtAccumulation[c][0]),
                        _mm256_add_epi32(_mm256_sub_epi32(_mm256_sub_epi32(prev_p, rem0_p), rem1_p), add_p));
                }
            }
            else {
#endif
            std::memcpy(accumulator.accumulation[c], prev_accumulator.accumulation[c], HalfDimensions * sizeof(std::int16_t));

#if defined(USE_AVX2)
            _mm256_store_si256(reinterpret_cast<__m256i*>(&accumulator.psqtAccumulation[c][0]),
                _mm256_load_si256(reinterpret_cast<const __m256i*>(&prev_accumulator.psqtAccumulation[c][0])));
#else
            for (std::size_t k = 0; k < PSQT_BUCKETS; ++k)
                accumulator.psqtAccumulation[c][k] = prev_accumulator.psqtAccumulation[c][k];
#endif

            for (std::uint32_t index = 0; index < pa.second; index++) {
                std::uint32_t offset = HalfDimensions * removed[index];

#if defined(USE_AVX2)
                {
                    auto* p = reinterpret_cast<__m256i*>(&accumulator.psqtAccumulation[c][0]);
                    *p = _mm256_sub_epi32(*p, _mm256_load_si256(reinterpret_cast<const __m256i*>(&psqts[removed[index] * PSQT_BUCKETS])));
                }
#else
                for (std::size_t k = 0; k < PSQT_BUCKETS; ++k)
                    accumulator.psqtAccumulation[c][k] -= psqts[removed[index] * PSQT_BUCKETS + k];
#endif

#if defined(USE_AVX2)
                auto column = reinterpret_cast<const __m256i*>(&weights[offset]);
                for (std::uint32_t j = 0; j < chunks; ++j)
                    accumulation[j] = _mm256_sub_epi16(accumulation[j], column[j]);
#endif
            }

            for (std::uint32_t index = 0; index < pa.first; index++) {
                std::uint32_t offset = HalfDimensions * added[index];

#if defined(USE_AVX2)
                {
                    auto* p = reinterpret_cast<__m256i*>(&accumulator.psqtAccumulation[c][0]);
                    *p = _mm256_add_epi32(*p, _mm256_load_si256(reinterpret_cast<const __m256i*>(&psqts[added[index] * PSQT_BUCKETS])));
                }
#else
                for (std::size_t k = 0; k < PSQT_BUCKETS; ++k)
                    accumulator.psqtAccumulation[c][k] += psqts[added[index] * PSQT_BUCKETS + k];
#endif

#if defined(USE_AVX2)
                auto column = reinterpret_cast<const __m256i*>(&weights[offset]);
                for (std::uint32_t j = 0; j < chunks; ++j)
                    accumulation[j] = _mm256_add_epi16(accumulation[j], column[j]);
#endif
            }
#if defined(USE_AVX2)
            }
#endif
        }
    }
}

inline void Transformer::refresh(Position & pos) {
    alignas(CACHE_LINE) std::uint32_t indexes[32];
    auto & accumulator = pos.state()->accumulator;

    for (COLOR c : { WHITE, BLACK }) {

        std::uint32_t ci = pos.getActiveIndexes(c, indexes);

#if defined(USE_AVX2)
        __m256i acc[NUM_REGS];
        __m256i psqt[NUM_PSQT_REGS];

        for (std::uint32_t j = 0; j < HalfDimensions / TILE_HEIGHT; ++j) {
            auto biasesTile = reinterpret_cast<const __m256i*>(&biases[j * TILE_HEIGHT]);
            for (std::uint32_t k = 0; k < NUM_REGS; ++k)
                acc[k] = biasesTile[k];

            for (std::uint32_t index = 0; index < ci; index++) {
                const std::uint32_t offset = HalfDimensions * indexes[index] + j * TILE_HEIGHT;
                auto column = reinterpret_cast<const __m256i*>(&weights[offset]);

                for (unsigned k = 0; k < NUM_REGS; ++k)
                    acc[k] = _mm256_add_epi16(acc[k], column[k]);
            }

            auto accTile = reinterpret_cast<__m256i*>(&accumulator.accumulation[c][j * TILE_HEIGHT]);
            for (unsigned k = 0; k < NUM_REGS; k++)
                _mm256_store_si256(&accTile[k], acc[k]);
        }

        for (std::uint32_t j = 0; j < PSQT_BUCKETS / PSQT_TILE_HEIGHT; ++j) {
            for (std::size_t k = 0; k < NUM_PSQT_REGS; ++k)
                psqt[k] = _mm256_setzero_si256();

            for (std::uint32_t index = 0; index < ci; index++) {
                const std::uint32_t offset = PSQT_BUCKETS * indexes[index] + j * PSQT_TILE_HEIGHT;
                auto columnPsqt = reinterpret_cast<const __m256i*>(&psqts[offset]);

                for (std::size_t k = 0; k < NUM_PSQT_REGS; ++k)
                    psqt[k] = _mm256_add_epi32(psqt[k], columnPsqt[k]);
            }

            auto accTilePsqt = reinterpret_cast<__m256i*>(&accumulator.psqtAccumulation[c][j * PSQT_TILE_HEIGHT]);

            for (std::size_t k = 0; k < NUM_PSQT_REGS; ++k)
                _mm256_store_si256(&accTilePsqt[k], psqt[k]);
        }
#endif
    }
}

template <std::int32_t OutputDimensions, std::int32_t InputDimensions>
Layer<OutputDimensions, InputDimensions>::Layer(std::istream & s) {
    memset(biases, 0, sizeof(biases));
    memset(weights, 0, sizeof(weights));

    s.read(reinterpret_cast<char*>(biases), sizeof(biases));
    s.read(reinterpret_cast<char*>(weights), sizeof(weights));
}

template <std::int32_t OutputDimensions, std::int32_t InputDimensions>
inline std::int32_t * Layer<OutputDimensions, InputDimensions>::propagate(std::uint8_t * features, char * outBuffer) {

    auto output = reinterpret_cast<std::int32_t*>(outBuffer);

#if defined(USE_AVX512)
    if constexpr (InputDimensions % (SIMD_WIDTH * 2) == 0) {
        const std::uint32_t chunks = InputDimensions / (SIMD_WIDTH * 2);
        const auto input_vector = reinterpret_cast<const __m512i*>(features);
        const __m512i ones = _mm512_set1_epi16(1);

        if constexpr (OutputDimensions % 4 == 0 && InputDimensions >= 128) {
            for (std::uint32_t i = 0; i < OutputDimensions; i += 4) {
                __m512i s0 = _mm512_setzero_si512();
                __m512i s1 = _mm512_setzero_si512();
                __m512i s2 = _mm512_setzero_si512();
                __m512i s3 = _mm512_setzero_si512();
                const auto r0 = reinterpret_cast<const __m512i*>(&weights[(i+0) * InputDimensions]);
                const auto r1 = reinterpret_cast<const __m512i*>(&weights[(i+1) * InputDimensions]);
                const auto r2 = reinterpret_cast<const __m512i*>(&weights[(i+2) * InputDimensions]);
                const auto r3 = reinterpret_cast<const __m512i*>(&weights[(i+3) * InputDimensions]);
                for (std::uint32_t j = 0; j < chunks; ++j) {
                    const __m512i inp = _mm512_loadu_si512(&input_vector[j]);
                    s0 = _mm512_add_epi32(s0, _mm512_madd_epi16(_mm512_maddubs_epi16(inp, _mm512_loadu_si512(&r0[j])), ones));
                    s1 = _mm512_add_epi32(s1, _mm512_madd_epi16(_mm512_maddubs_epi16(inp, _mm512_loadu_si512(&r1[j])), ones));
                    s2 = _mm512_add_epi32(s2, _mm512_madd_epi16(_mm512_maddubs_epi16(inp, _mm512_loadu_si512(&r2[j])), ones));
                    s3 = _mm512_add_epi32(s3, _mm512_madd_epi16(_mm512_maddubs_epi16(inp, _mm512_loadu_si512(&r3[j])), ones));
                }
                output[i+0] = _mm512_reduce_add_epi32(s0) + biases[i+0];
                output[i+1] = _mm512_reduce_add_epi32(s1) + biases[i+1];
                output[i+2] = _mm512_reduce_add_epi32(s2) + biases[i+2];
                output[i+3] = _mm512_reduce_add_epi32(s3) + biases[i+3];
            }
            return output;
        }

        for (std::uint32_t i = 0; i < OutputDimensions; ++i) {
            __m512i sum = _mm512_setzero_si512();
            const auto row = reinterpret_cast<const __m512i*>(&weights[i * InputDimensions]);
            for (std::uint32_t j = 0; j < chunks; ++j) {
                __m512i product = _mm512_maddubs_epi16(_mm512_loadu_si512(&input_vector[j]), _mm512_loadu_si512(&row[j]));
                product = _mm512_madd_epi16(product, ones);
                sum = _mm512_add_epi32(sum, product);
            }
            output[i] = _mm512_reduce_add_epi32(sum) + biases[i];
        }
        return output;
    }
#endif

#if defined(USE_AVX2)
    std::uint32_t chunks = InputDimensions / SIMD_WIDTH;
    const __m256i ones = _mm256_set1_epi16(1);
    const auto input_vector = reinterpret_cast<const __m256i*>(features);
#endif

#if defined(USE_AVX2)
    // 4-way kernel blocking: process 4 output neurons simultaneously.
    // 4 independent accumulator chains allow better ILP vs. one serial chain per output.
    if constexpr (OutputDimensions % 4 == 0 && InputDimensions >= 128) {
        for (std::uint32_t i = 0; i < OutputDimensions; i += 4) {
            __m256i s0 = _mm256_setzero_si256();
            __m256i s1 = _mm256_setzero_si256();
            __m256i s2 = _mm256_setzero_si256();
            __m256i s3 = _mm256_setzero_si256();
            const auto r0 = reinterpret_cast<const __m256i*>(&weights[(i+0) * InputDimensions]);
            const auto r1 = reinterpret_cast<const __m256i*>(&weights[(i+1) * InputDimensions]);
            const auto r2 = reinterpret_cast<const __m256i*>(&weights[(i+2) * InputDimensions]);
            const auto r3 = reinterpret_cast<const __m256i*>(&weights[(i+3) * InputDimensions]);
            for (std::uint32_t j = 0; j < chunks; ++j) {
                const __m256i inp = _mm256_loadA_si256(&input_vector[j]);
                s0 = _mm256_add_epi32(s0, _mm256_madd_epi16(_mm256_maddubs_epi16(inp, _mm256_load_si256(&r0[j])), ones));
                s1 = _mm256_add_epi32(s1, _mm256_madd_epi16(_mm256_maddubs_epi16(inp, _mm256_load_si256(&r1[j])), ones));
                s2 = _mm256_add_epi32(s2, _mm256_madd_epi16(_mm256_maddubs_epi16(inp, _mm256_load_si256(&r2[j])), ones));
                s3 = _mm256_add_epi32(s3, _mm256_madd_epi16(_mm256_maddubs_epi16(inp, _mm256_load_si256(&r3[j])), ones));
            }
            auto hreduce = [](const __m256i& s) -> std::int32_t {
                __m128i t = _mm_add_epi32(_mm256_castsi256_si128(s), _mm256_extracti128_si256(s, 1));
                t = _mm_add_epi32(t, _mm_shuffle_epi32(t, _MM_PERM_BADC));
                return _mm_cvtsi128_si32(_mm_add_epi32(t, _mm_shuffle_epi32(t, _MM_PERM_CDAB)));
            };
            output[i+0] = hreduce(s0) + biases[i+0];
            output[i+1] = hreduce(s1) + biases[i+1];
            output[i+2] = hreduce(s2) + biases[i+2];
            output[i+3] = hreduce(s3) + biases[i+3];
        }
        return output;
    }
#endif

    for (std::uint32_t i = 0; i < OutputDimensions; ++i) {
        const std::uint32_t offset = i * InputDimensions;

#if defined(USE_AVX2)
        __m256i sum = _mm256_setzero_si256();
        const auto row = reinterpret_cast<const __m256i*>(&weights[offset]);
        for (std::uint32_t j = 0; j < chunks; ++j) {
            __m256i product = _mm256_maddubs_epi16(_mm256_loadA_si256(&input_vector[j]), _mm256_load_si256(&row[j]));
            product = _mm256_madd_epi16(product, ones);
            sum = _mm256_add_epi32(sum, product);
        }
        __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(sum), _mm256_extracti128_si256(sum, 1));
        sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_PERM_BADC));
        sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_PERM_CDAB));
        output[i] = _mm_cvtsi128_si32(sum128) + biases[i];
#endif
    }

    return output;
}

LayeredNetwork::LayeredNetwork(std::istream & s) : inputLayer(s), hiddenLayer1(s), hiddenLayer2(s) {
}

inline std::int32_t * LayeredNetwork::propagate(std::uint8_t * features, char * outBuffer) {
    auto ret = inputLayer.propagate(features, outBuffer + 320);                 // forward propagation
    auto fc_0_out = ret[15];
    char ac_sqr_0_out[32] = { 0 };
    ClippedReLU<6, 16>::propagateSqrt(ret, ac_sqr_0_out);                       // clip
    char ac_out[32];
    auto clipped = ClippedReLU<6, 16>::propagate(ret, ac_out);
    std::memcpy(ac_sqr_0_out + 15, ac_out, 15);
    ret = hiddenLayer1.propagate((std::uint8_t*)ac_sqr_0_out, outBuffer + 128); // forward propagation
    clipped = ClippedReLU<6, 32>::propagate(ret, outBuffer + 64);               // clip
    ret = hiddenLayer2.propagate(clipped, outBuffer);                           // forward propagation

    std::int32_t fwdOut = int(fc_0_out) * (600 * 16) / (127 * (1 << 6));
    std::int32_t outputValue = ret[0] + fwdOut;
    ret[0] = outputValue;

    return ret;
}

template <std::int32_t WeightScaleBits, std::int32_t InputDimensions>
inline std::uint8_t* ClippedReLU<WeightScaleBits, InputDimensions>::propagate(std::int32_t* features, char* outBuffer) {

    auto output = reinterpret_cast<uint8_t*>(outBuffer);

#if defined(USE_AVX2)
    auto chunks = InputDimensions / SIMD_WIDTH;
    const __m256i zero = _mm256_setzero_si256();
    const __m256i offsets = _mm256_set_epi32(7, 3, 6, 2, 5, 1, 4, 0);
    const auto in = reinterpret_cast<const __m256i*>(features);
    const auto out = reinterpret_cast<__m256i*>(output);

    for (auto i = 0; i < chunks; ++i) {
        const __m256i words0 = _mm256_srai_epi16(_mm256_packs_epi32(_mm256_loadA_si256(&in[i * 4 + 0]), _mm256_loadA_si256(&in[i * 4 + 1])), WeightScaleBits);
        const __m256i words1 = _mm256_srai_epi16(_mm256_packs_epi32(_mm256_loadA_si256(&in[i * 4 + 2]), _mm256_loadA_si256(&in[i * 4 + 3])), WeightScaleBits);
        _mm256_storeA_si256(&out[i], _mm256_permutevar8x32_epi32(_mm256_max_epi8(_mm256_packs_epi16(words0, words1), zero), offsets));
    }
#endif

    std::uint32_t start = chunks * SIMD_WIDTH;

    for (std::uint32_t i = start; i < InputDimensions; ++i)
        output[i] = static_cast<std::uint8_t>(std::max(0, std::min(127, features[i] >> WeightScaleBits)));

    return output;
}

template <std::int32_t WeightScaleBits, std::int32_t InputDimensions>
inline std::uint8_t* ClippedReLU<WeightScaleBits, InputDimensions>::propagateSqrt(std::int32_t* features, char* outBuffer)
{
    auto output = reinterpret_cast<uint8_t*>(outBuffer);

#if defined(USE_AVX2)
    if constexpr (InputDimensions == 16) {
        const auto in_vec = reinterpret_cast<const __m256i*>(features);
        __m256i v0 = _mm256_load_si256(&in_vec[0]);   // features[0..7]
        __m256i v1 = _mm256_load_si256(&in_vec[1]);   // features[8..15]

        // Clamp to [-46340, 46340]: 46340^2 = 2,147,388,400 < INT32_MAX, no overflow in mullo_epi32
        const __m256i clamp_lo = _mm256_set1_epi32(-46340);
        const __m256i clamp_hi = _mm256_set1_epi32(46340);
        v0 = _mm256_min_epi32(_mm256_max_epi32(v0, clamp_lo), clamp_hi);
        v1 = _mm256_min_epi32(_mm256_max_epi32(v1, clamp_lo), clamp_hi);

        // Square (no int32 overflow after clamp)
        v0 = _mm256_mullo_epi32(v0, v0);
        v1 = _mm256_mullo_epi32(v1, v1);

        // Shift right by 2*WeightScaleBits+7: equivalent to >>WeightScaleBits*2 then /128
        v0 = _mm256_srli_epi32(v0, 2 * WeightScaleBits + 7);
        v1 = _mm256_srli_epi32(v1, 2 * WeightScaleBits + 7);

        // Clamp to [0, 127]: values are non-negative (squares), only upper bound needed
        const __m256i out_max = _mm256_set1_epi32(127);
        v0 = _mm256_min_epi32(v0, out_max);
        v1 = _mm256_min_epi32(v1, out_max);

        // Pack int32->int16->uint8, fixing AVX2 lane interleaving with a permute
        __m256i packed16 = _mm256_permute4x64_epi64(
            _mm256_packs_epi32(v0, v1), 0b11011000);   // reorders to [v0[0..7], v1[0..7]]

        __m256i packed8 = _mm256_packus_epi16(packed16, _mm256_setzero_si256());
        // Layout: [v0[0..7] | zeros | v1[0..7] | zeros]; combine the two halves:
        __m128i combined = _mm_unpacklo_epi64(
            _mm256_castsi256_si128(packed8),
            _mm256_extracti128_si256(packed8, 1));      // [v0[0..7], v1[0..7]] = 16 bytes

        _mm_storeu_si128(reinterpret_cast<__m128i*>(outBuffer), combined);
        return output;
    }
#endif

    for (std::uint32_t i = 0; i < InputDimensions; ++i)
        output[i] = static_cast<std::uint8_t>(std::max(0ll, std::min(127ll, (((long long)features[i] * features[i]) >> (2 * WeightScaleBits)) / 128)));

    return output;
}