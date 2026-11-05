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
#include "numa.h"
#include "position.h"
#include "hce.h"
#include "utils.h"
#include <immintrin.h>
#include <algorithm>
#include <new>
#include <cstdlib>
#include <streambuf>
#include <fstream>
#include <iostream>
#include <atomic>
#include <array>
#include <vector>

#if defined(__linux__) && !defined(__ANDROID__)
#include <sys/mman.h>
#endif

#if defined(_WIN32)
#include <malloc.h>
#endif

#if !defined(PURE_HCE)
#include "incbin/incbin.h"

#if !defined(_MSC_VER)
INCBIN(EmbeddedNNUE, EVALFILE);
#endif // _MSC_VER

//
//  The net that threads read: one copy, or one for each NUMA node when threads are bound to nodes
//

struct NetCopy {
    TransformerPtr transformer;
    std::unique_ptr<LayeredNetwork> networks[LAYERED_NETWORKS];
};

static std::vector<NetCopy> s_nets(1);

struct ThreadNet {
    Transformer * transformer;
    LayeredNetwork * networks[LAYERED_NETWORKS];
};

#if defined(NUMA_BINDING)
static thread_local ThreadNet s_threadNet;
#else
static ThreadNet s_threadNet;
#endif

// bumped on every network (re)load so per-thread refresh caches drop stale columns
static std::atomic<int> s_networkGeneration{0};
#endif // PURE_HCE

/*static */EVAL Evaluator::bound(EVAL v) {
    return std::max(-DECISIVE_SCORE + 1, std::min(v, DECISIVE_SCORE - 1));
}

static FORCE_INLINE EVAL bounded(EVAL v) { return Evaluator::bound(v); }

EVAL Evaluator::evaluateRaw(Position & pos) {
#if defined(PURE_HCE)
    // Pure hand-crafted evaluation, selected at compile time with -DPURE_HCE.
    Pair base = Hce::baseScore(pos);
    return bounded(Hce::evaluate(pos, base));
#else
    EVAL scale = 600 + 20 * pos.nonPawnMaterial() / 1024;
    return bounded(static_cast<EVAL>(NnueEvaluate(pos) * scale / 1024));
#endif
}

/*static */EVAL Evaluator::fromRaw(EVAL raw, int fifty) {
#if defined(PURE_HCE)
    (void)fifty;
    return bounded(raw);
#else
    return bounded(raw * (208 - fifty) / 208 + Tempo);
#endif
}

EVAL Evaluator::evaluate(Position & pos) {
    return fromRaw(evaluateRaw(pos), pos.Fifty());
}

//
//  Each copy is written by a thread bound to its node, which is what puts its pages there
//

/*static */void Evaluator::replicateNet(bool rebuild) {
#if defined(PURE_HCE)
    (void)rebuild;
#else
    const unsigned int nodes = Numa::boundNodes();

    if (rebuild || nodes == 1)
        s_nets.resize(1);

    if (s_nets.size() != nodes) {
        std::vector<NetCopy> copies(nodes);

        // some unit tests set threads without a net, then there is nothing to copy
        for (unsigned int node = 0; s_nets[0].transformer && node < nodes; ++node) {
            Numa::runOnNode(node, [&copies, node]() {
                copies[node].transformer = makeTransformer(*s_nets[0].transformer);

                for (int i = 0; i < LAYERED_NETWORKS; ++i)
                    copies[node].networks[i].reset(new LayeredNetwork(*s_nets[0].networks[i]));
            });

            if (!copies[node].transformer) {
                std::cout << "Fatal error: unable to allocate memory for a copy of the network" << std::endl;
                exit(1);
            }
        }

        s_nets = std::move(copies);
    }

    useNet();
#endif
}

/*static */void Evaluator::useNet() {
#if !defined(PURE_HCE)
    const NetCopy & net = s_nets[Numa::threadNode];

    s_threadNet.transformer = net.transformer.get();

    for (int i = 0; i < LAYERED_NETWORKS; ++i)
        s_threadNet.networks[i] = net.networks[i].get();
#endif
}

#if !defined(PURE_HCE)
bool Evaluator::initEval()
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

    return initEval(stream);
}

/*static */bool Evaluator::initEval(std::istream & stream) {

    std::uint32_t version, size, hash_value;
    stream.read(reinterpret_cast<char*>(&version), sizeof(version));
    stream.read(reinterpret_cast<char*>(&hash_value), sizeof(hash_value));
    stream.read(reinterpret_cast<char*>(&size), sizeof(size));

    if (!stream || size > 1024)
        return false;

    std::string architecture;
    architecture.resize(size);
    stream.read(&(architecture)[0], size);

    NetCopy net;
    net.transformer = makeTransformer(stream);

    if (!net.transformer)
        return false;

    for (auto i = 0; i < LAYERED_NETWORKS; ++i) {
        stream.read(reinterpret_cast<char*>(&version), sizeof(version));
        net.networks[i].reset(new LayeredNetwork(stream));
    }

    s_nets.clear();
    s_nets.push_back(std::move(net));
    replicateNet(false);

    ++s_networkGeneration;

    return stream.good() && stream.peek() == std::ios::traits_type::eof();
}

/*static */bool Evaluator::setEvalFile(const std::string & evalFile)
{
    if (evalFile.empty() || evalFile == "<empty>") {
        initEval();
        std::cout << "info string default network loaded" << std::endl;
        return true;
    }

    std::ifstream stream(evalFile, std::ios::binary);

    if (stream && initEval(stream)) {
        std::cout << "info string network " << evalFile << " loaded" << std::endl;
        return true;
    }

    initEval();
    std::cout << "info string unable to load network " << evalFile << ", default network restored" << std::endl;
    return false;
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

    auto psqt = s_threadNet.transformer->transform(pos, features, bucket);

    //
    // call network evaluation
    //

    alignas(CACHE_LINE) char buffer[384];
    auto output = s_threadNet.networks[bucket]->propagate(features, buffer);

    //
    // scale the result
    //

    int eval      = output[0];
    int delta     = 7;

    accumulator.score = static_cast<int>(((128 - delta) * psqt + (128 + delta) * eval) / 128 / WEIGHTS_SCALE);
    accumulator.computed_score = true;

    return accumulator.score;
}

//
// Allocate the transformer on huge pages
//

static constexpr std::size_t HugePage = 2 * 1024 * 1024;

static void * allocTransformer() {

    const std::size_t bytes = (sizeof(Transformer) + HugePage - 1) / HugePage * HugePage;

#if defined(_WIN32)
    void * raw = _aligned_malloc(bytes, HugePage);
#else
    void * raw = std::aligned_alloc(HugePage, bytes);
#endif

#if defined(__linux__) && !defined(__ANDROID__)
    // same idea as the transposition table, see TTable::setHashSize
    if (raw)
        madvise(raw, bytes, MADV_HUGEPAGE);
#endif

    return raw;
}

TransformerPtr makeTransformer(std::istream & s) {
    void * raw = allocTransformer();
    return raw ? TransformerPtr(new (raw) Transformer(s)) : TransformerPtr();
}

TransformerPtr makeTransformer(const Transformer & source) {
    void * raw = allocTransformer();
    return raw ? TransformerPtr(new (raw) Transformer(source)) : TransformerPtr();
}

void TransformerDeleter::operator()(Transformer * t) const noexcept {

    if (!t)
        return;

    t->~Transformer();

#if defined(_WIN32)
    _aligned_free(t);
#else
    std::free(t);
#endif
}

static void readStrided(std::istream & s, std::int8_t * out, std::size_t rows, std::size_t width, std::size_t stride) {

    for (std::size_t r = 0; r < rows; ++r)
        s.read(reinterpret_cast<char*>(out + r * stride), width);
}

Transformer::Transformer(std::istream & s) {
    std::uint32_t header;
    s.read(reinterpret_cast<char*>(&header), sizeof(header));
    s.read(reinterpret_cast<char*>(biases), sizeof(biases));

    if (header == ThreatHeader) {
        readStrided(s, threatBlock, ThreatDimensions, HalfDimensions, ThreatStride);
        s.read(reinterpret_cast<char*>(weights), sizeof(weights));
        readStrided(s, threatBlock + ThreatPsqtOffset, ThreatDimensions, PSQT_BUCKETS * sizeof(std::int32_t), ThreatStride);
        s.read(reinterpret_cast<char*>(psqts), sizeof(psqts));
    }
    else {
        s.read(reinterpret_cast<char*>(weights), sizeof(weights));
        s.read(reinterpret_cast<char*>(psqts), sizeof(psqts));

        std::memset(threatBlock, 0, sizeof(threatBlock));
    }
}

static const ThreatList EmptyThreats{};

static inline void applyDelta(const Transformer & t, std::int16_t * out, const std::int16_t * in, std::int32_t * psqtOut, const std::int32_t * psqtIn,
                              const std::uint32_t * removed, std::uint32_t removedCount, const std::uint32_t * added, std::uint32_t addedCount,
                              const ThreatList & thrRemoved, const ThreatList & thrAdded) {

#if defined(USE_AVX2)
#if defined(USE_AVX512)
    using acc_vec_t = __m512i;
    using col_vec_t = __m256i;

    auto vadd16  = [](acc_vec_t a, acc_vec_t b) { return _mm512_add_epi16(a, b); };
    auto vsub16  = [](acc_vec_t a, acc_vec_t b) { return _mm512_sub_epi16(a, b); };
    auto vwiden8 = [](const col_vec_t * c) { return _mm512_cvtepi8_epi16(_mm256_load_si256(c)); };

    constexpr std::uint32_t Regs = NUM_REGS;
#else
    using acc_vec_t = __m256i;
    using col_vec_t = __m128i;

    auto vadd16  = [](acc_vec_t a, acc_vec_t b) { return _mm256_add_epi16(a, b); };
    auto vsub16  = [](acc_vec_t a, acc_vec_t b) { return _mm256_sub_epi16(a, b); };
    auto vwiden8 = [](const col_vec_t * c) { return _mm256_cvtepi8_epi16(_mm_load_si128(c)); };

    constexpr std::uint32_t Regs = NUM_REGS / 2;
#endif

    constexpr std::uint32_t TileHeight = Regs * sizeof(acc_vec_t) / sizeof(std::int16_t);
    constexpr std::uint32_t NumTiles   = Transformer::HalfDimensions / TileHeight;

    static_assert(Transformer::HalfDimensions % TileHeight == 0);

    const std::int8_t * thrColumns[2 * ThreatList::MAX_LENGTH];
    const std::size_t   thrRemovedCount = thrRemoved.size();
    const std::size_t   thrCount        = thrRemovedCount + thrAdded.size();

    for (std::size_t i = 0; i < thrRemovedCount; ++i)
        thrColumns[i] = t.threatWeights(thrRemoved[i]);

    for (std::size_t i = 0; i < thrAdded.size(); ++i)
        thrColumns[thrRemovedCount + i] = t.threatWeights(thrAdded[i]);

    for (std::uint32_t tile = 0; tile < NumTiles; ++tile) {

        const std::uint32_t offset = tile * TileHeight;
        const auto inTile  = reinterpret_cast<const acc_vec_t*>(in + offset);
        auto       outTile = reinterpret_cast<acc_vec_t*>(out + offset);

        acc_vec_t acc[Regs];

        for (std::uint32_t k = 0; k < Regs; ++k)
            acc[k] = inTile[k];

        for (std::uint32_t i = 0; i < removedCount; ++i) {
            auto column = reinterpret_cast<const acc_vec_t*>(&t.weights[Transformer::HalfDimensions * removed[i] + offset]);
            for (std::uint32_t k = 0; k < Regs; ++k)
                acc[k] = vsub16(acc[k], column[k]);
        }

        for (std::uint32_t i = 0; i < addedCount; ++i) {
            auto column = reinterpret_cast<const acc_vec_t*>(&t.weights[Transformer::HalfDimensions * added[i] + offset]);
            for (std::uint32_t k = 0; k < Regs; ++k)
                acc[k] = vadd16(acc[k], column[k]);
        }

        for (std::size_t i = 0; i < thrRemovedCount; ++i) {
            auto column = reinterpret_cast<const col_vec_t*>(thrColumns[i] + offset);
            for (std::uint32_t k = 0; k < Regs; ++k)
                acc[k] = vsub16(acc[k], vwiden8(&column[k]));
        }

        for (std::size_t i = thrRemovedCount; i < thrCount; ++i) {
            auto column = reinterpret_cast<const col_vec_t*>(thrColumns[i] + offset);
            for (std::uint32_t k = 0; k < Regs; ++k)
                acc[k] = vadd16(acc[k], vwiden8(&column[k]));
        }

        for (std::uint32_t k = 0; k < Regs; ++k)
            outTile[k] = acc[k];
    }

    __m256i psqt = _mm256_load_si256(reinterpret_cast<const __m256i*>(psqtIn));

    for (std::uint32_t i = 0; i < removedCount; ++i)
        psqt = _mm256_sub_epi32(psqt, _mm256_load_si256(reinterpret_cast<const __m256i*>(&t.psqts[removed[i] * PSQT_BUCKETS])));

    for (std::uint32_t i = 0; i < addedCount; ++i)
        psqt = _mm256_add_epi32(psqt, _mm256_load_si256(reinterpret_cast<const __m256i*>(&t.psqts[added[i] * PSQT_BUCKETS])));

    for (std::size_t i = 0; i < thrRemovedCount; ++i)
        psqt = _mm256_sub_epi32(psqt, _mm256_load_si256(reinterpret_cast<const __m256i*>(t.threatPsqts(thrRemoved[i]))));

    for (std::size_t i = 0; i < thrAdded.size(); ++i)
        psqt = _mm256_add_epi32(psqt, _mm256_load_si256(reinterpret_cast<const __m256i*>(t.threatPsqts(thrAdded[i]))));

    _mm256_store_si256(reinterpret_cast<__m256i*>(psqtOut), psqt);
#else
    (void)thrRemoved;
    (void)thrAdded;

    std::memcpy(out, in, Transformer::HalfDimensions * sizeof(std::int16_t));

    for (std::size_t k = 0; k < PSQT_BUCKETS; ++k)
        psqtOut[k] = psqtIn[k];

    for (std::uint32_t i = 0; i < removedCount; ++i)
        for (std::size_t k = 0; k < PSQT_BUCKETS; ++k)
            psqtOut[k] -= t.psqts[removed[i] * PSQT_BUCKETS + k];

    for (std::uint32_t i = 0; i < addedCount; ++i)
        for (std::size_t k = 0; k < PSQT_BUCKETS; ++k)
            psqtOut[k] += t.psqts[added[i] * PSQT_BUCKETS + k];
#endif
}

inline __m256i vec_msb_pack_16(__m256i a, __m256i b) {
    __m256i compacted = _mm256_packs_epi16(_mm256_srli_epi16(a, 7), _mm256_srli_epi16(b, 7));
    return _mm256_permute4x64_epi64(compacted, 0b11011000);
}

#if defined(USE_AVX2)
inline __m256i affine_acc_256(__m256i acc, __m256i a, __m256i b) {
#if defined(USE_AVXVNNI)
    return _mm256_dpbusd_epi32(acc, a, b);
#else
    const __m256i ones = _mm256_set1_epi16(1);
    return _mm256_add_epi32(acc, _mm256_madd_epi16(_mm256_maddubs_epi16(a, b), ones));
#endif
}
#endif

#if defined(USE_AVX512)
inline __m512i vec_msb_pack_16_512(__m512i a, __m512i b) {
    __m512i compacted = _mm512_packs_epi16(_mm512_srli_epi16(a, 7), _mm512_srli_epi16(b, 7));
    const __m512i idx = _mm512_setr_epi64(0, 2, 4, 6, 1, 3, 5, 7);
    return _mm512_permutexvar_epi64(idx, compacted);
}

inline __m512i affine_acc_512(__m512i acc, __m512i a, __m512i b) {
#if defined(USE_VNNI)
    return _mm512_dpbusd_epi32(acc, a, b);
#else
    const __m512i ones = _mm512_set1_epi16(1);
    return _mm512_add_epi32(acc, _mm512_madd_epi16(_mm512_maddubs_epi16(a, b), ones));
#endif
}
#endif

inline __m128i m256_haddx4(__m256i s0, __m256i s1, __m256i s2, __m256i s3, __m128i bias) {
    s0 = _mm256_hadd_epi32(s0, s1);
    s2 = _mm256_hadd_epi32(s2, s3);
    s0 = _mm256_hadd_epi32(s0, s2);
    const __m128i lo = _mm256_castsi256_si128(s0);
    const __m128i hi = _mm256_extracti128_si256(s0, 1);
    return _mm_add_epi32(_mm_add_epi32(lo, hi), bias);
}

#if defined(USE_AVX512)
inline __m128i m512_haddx4(__m512i s0, __m512i s1, __m512i s2, __m512i s3, __m128i bias) {
    const __m256i a0 = _mm256_add_epi32(_mm512_castsi512_si256(s0), _mm512_extracti64x4_epi64(s0, 1));
    const __m256i a1 = _mm256_add_epi32(_mm512_castsi512_si256(s1), _mm512_extracti64x4_epi64(s1, 1));
    const __m256i a2 = _mm256_add_epi32(_mm512_castsi512_si256(s2), _mm512_extracti64x4_epi64(s2, 1));
    const __m256i a3 = _mm256_add_epi32(_mm512_castsi512_si256(s3), _mm512_extracti64x4_epi64(s3, 1));
    return m256_haddx4(a0, a1, a2, a3, bias);
}
#endif

static constexpr std::size_t CatchUpLimit = 8;

static bool movesKing(const Undo & state, COLOR c) {
    const auto & dp = state.dirtyPiece;
    return dp.dirty_num && dp.pieceId[0] == PIECE_ID_KING + c;
}

std::int32_t Transformer::transform(Position & pos, std::uint8_t * outBuffer, const std::size_t bucket) {

    const auto s = pos.state();

    for (COLOR c : { WHITE, BLACK })
        if (!s->accumulator.computed_accumulation[c])
            update(pos, c);

    auto & acc  = s->accumulator.accumulation;
    auto & psqt = s->accumulator.psqtAccumulation;

    const Color sides[2] = { pos.Side(), !pos.Side() };
    const auto pt = (psqt[static_cast<int>(sides[0])][bucket] - psqt[static_cast<int>(sides[1])][bucket]) / 2;

    for (std::uint32_t side = 0; side < 2; ++side) {
        std::uint32_t offset = (HalfDimensions / 2) * side;

#if defined(USE_AVX512)
        constexpr std::uint32_t OutputChunkSize = MAX_CHUNK_SIZE * 2;
        static_assert((HalfDimensions / 2) % OutputChunkSize == 0);
        constexpr std::uint32_t NumOutputChunks = HalfDimensions / 2 / OutputChunkSize;

        const __m512i Zero = _mm512_setzero_si512();
        const __m512i One = _mm512_set1_epi16(127);

        const __m512i* in0 = reinterpret_cast<const __m512i*>(&(acc[sides[side]][0]));
        const __m512i* in1 = reinterpret_cast<const __m512i*>(&(acc[sides[side]][HalfDimensions / 2]));
        __m512i* out = reinterpret_cast<__m512i*>(outBuffer + offset);

        for (std::uint32_t j = 0; j < NumOutputChunks; j += 1) {
            const __m512i sum0a = _mm512_max_epi16(_mm512_min_epi16(in0[j * 2 + 0], One), Zero);
            const __m512i sum0b = _mm512_max_epi16(_mm512_min_epi16(in0[j * 2 + 1], One), Zero);
            const __m512i sum1a = _mm512_max_epi16(_mm512_min_epi16(in1[j * 2 + 0], One), Zero);
            const __m512i sum1b = _mm512_max_epi16(_mm512_min_epi16(in1[j * 2 + 1], One), Zero);

            const __m512i pa = _mm512_mullo_epi16(sum0a, sum1a);
            const __m512i pb = _mm512_mullo_epi16(sum0b, sum1b);

            out[j] = vec_msb_pack_16_512(pa, pb);
        }
#elif defined(USE_AVX2)
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

//
// Per-thread accumulator refresh cache: one entry per perspective and own-king square,
// holding the accumulator last built for that king placement and the piece list it was
// built from. A refresh then costs only the changed piece columns instead of a full
// rebuild from biases (~30 columns), which pays off because any king move invalidates
// every feature index of its perspective.
//

struct RefreshEntry {
    alignas(CACHE_LINE) std::int16_t accumulation[Transformer::HalfDimensions];
    alignas(CACHE_LINE) std::int32_t psqt[PSQT_BUCKETS];
    PieceSquare pieces[EvalList::MAX_LENGTH];
    bool valid;
};

//
//  A thread local with an initialiser puts a guard check on every use of every thread local in this file, the
//  net pointers too. So generation has none: it starts at 0, which differs from s_networkGeneration once a net is loaded
//

struct RefreshTable {
    RefreshEntry entry[COLOR_NB][SQUARE_NB];
    int generation;
};

static thread_local RefreshTable s_refreshTable;

static void refreshPerspective(Transformer & t, Position & pos, COLOR c) {

    auto & table = s_refreshTable;

    if (table.generation != s_networkGeneration.load(std::memory_order_relaxed)) {
        for (auto & perspective : table.entry)
            for (auto & e : perspective)
                e.valid = false;
        table.generation = s_networkGeneration.load(std::memory_order_relaxed);
    }

    auto & entry       = table.entry[c][pos.King(c)];
    auto & accumulator = pos.state()->accumulator;
    const auto pieces  = c == WHITE ? pos.eval_list()->piece_list_fw() : pos.eval_list()->piece_list_fb();

    if (!entry.valid) {
        std::memcpy(entry.accumulation, t.biases, Transformer::HalfDimensions * sizeof(std::int16_t));

        for (std::size_t k = 0; k < PSQT_BUCKETS; ++k)
            entry.psqt[k] = 0;

        for (int i = 0; i < EvalList::MAX_LENGTH; ++i)
            entry.pieces[i] = PS_NONE;

        entry.valid = true;
    }

    // Orient constants are fixed for a cache entry (same logic as getActiveIndexes)
    const PieceId target = static_cast<PieceId>(PIECE_ID_KING + c);
    Square kingSq = static_cast<Square>((pieces[target] - PS_KING) % SQUARE_NB);
    kingSq = FLIP[c][kingSq];
    const int flip_mask = (bool(c) * SQ_A8) ^ ((Col(kingSq) < FILE_E) * SQ_H1);
    const std::uint32_t king_bucket_offset = PS_END * KingBuckets[Square(int(kingSq) ^ flip_mask)];

    alignas(CACHE_LINE) std::uint32_t added[EvalList::MAX_LENGTH];
    alignas(CACHE_LINE) std::uint32_t removed[EvalList::MAX_LENGTH];
    std::uint32_t ca = 0;
    std::uint32_t cr = 0;

    //
    // Find the changed slots with one compare per vector
    //

    static_assert(sizeof(PieceSquare) == 4 && EvalList::MAX_LENGTH == 32);

    U64 changedSlots = 0;

#if defined(USE_AVX512)
    for (int b = 0; b < 2; ++b)
        changedSlots |= U64(_mm512_cmpneq_epi32_mask(_mm512_loadu_si512(&entry.pieces[16 * b]), _mm512_loadu_si512(&pieces[16 * b]))) << (16 * b);
#else
    for (int b = 0; b < 4; ++b) {
        const __m256i same = _mm256_cmpeq_epi32(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(&entry.pieces[8 * b])), _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&pieces[8 * b])));
        changedSlots |= U64(~_mm256_movemask_ps(_mm256_castsi256_ps(same)) & 0xFF) << (8 * b);
    }
#endif

    while (changedSlots) {
        const int  i     = PopLSB(changedSlots) ^ 63;
        const auto oldPs = entry.pieces[i];
        const auto newPs = pieces[i];

        if (oldPs != PS_NONE) {
            const Square sq = FLIP[c][static_cast<Square>((oldPs - PS_KING) % SQUARE_NB)];
            removed[cr++] = static_cast<std::uint32_t>((int(sq) ^ flip_mask) + (oldPs - oldPs % SQUARE_NB) + king_bucket_offset);
        }

        if (newPs != PS_NONE) {
            const Square sq = FLIP[c][static_cast<Square>((newPs - PS_KING) % SQUARE_NB)];
            added[ca++] = static_cast<std::uint32_t>((int(sq) ^ flip_mask) + (newPs - newPs % SQUARE_NB) + king_bucket_offset);
        }

        entry.pieces[i] = newPs;
    }

    if (ca || cr)
        applyDelta(t, entry.accumulation, entry.accumulation, entry.psqt, entry.psqt, removed, cr, added, ca, EmptyThreats, EmptyThreats);

    //
    // the accumulator is the cached king-relative part with every active threat on top, built in one pass
    //

    ThreatList active;
    getActiveThreatIndexes(pos, c, active);
    applyDelta(t, accumulator.accumulation[c], entry.accumulation, accumulator.psqtAccumulation[c], entry.psqt, nullptr, 0, nullptr, 0, EmptyThreats, active);
}

inline void Transformer::incremental(Position & pos, COLOR c, Accumulator & accumulator, const Accumulator & prev_accumulator, const Undo & move, bool backward) {
    const auto & dp = move.dirtyPiece;

    alignas(CACHE_LINE) std::uint32_t added[32];
    alignas(CACHE_LINE) std::uint32_t removed[32];

    std::pair<std::uint32_t, std::uint32_t> pa{};

    if (dp.dirty_num) { // a piece moved
        assert(dp.pieceId[0] != PIECE_ID_KING + c); // a move of this king is a refresh of this perspective, never an update
        pa = pos.getChangedIndexes(c, dp, backward ? removed : added, backward ? added : removed);

        if (backward)
            std::swap(pa.first, pa.second);
    }

    ThreatList thrRemoved, thrAdded;
    getChangedThreatIndexes(c, pos.King(c), move.dirtyThreats, backward ? thrAdded : thrRemoved, backward ? thrRemoved : thrAdded);

    applyDelta(*this, accumulator.accumulation[c], prev_accumulator.accumulation[c], accumulator.psqtAccumulation[c], prev_accumulator.psqtAccumulation[c],
               removed, pa.second, added, pa.first, thrRemoved, thrAdded);
}

inline void Transformer::update(Position & pos, COLOR c) {

    const auto s = pos.state();

    Undo * pending[CatchUpLimit];
    std::size_t count = 0;
    Undo * base = movesKing(*s, c) ? nullptr : s->previous;

    while (base && !base->accumulator.computed_accumulation[c]) {

        if (base->dirtyPiece.dirty_num) {

            if (count == CatchUpLimit || movesKing(*base, c))
                break;

            pending[count++] = base;
        }

        base = base->previous;
    }

    if (base && base->accumulator.computed_accumulation[c]) {

        while (count--) {
            incremental(pos, c, pending[count]->accumulator, base->accumulator, *pending[count], false);
            pending[count]->accumulator.computed_accumulation[c] = true;
            pending[count]->accumulator.computed_score = false;
            base = pending[count];
        }

        incremental(pos, c, s->accumulator, base->accumulator, *s, false);
    }
    else {
        refreshPerspective(*this, pos, c);

        for (std::size_t i = 0; i < count; ++i) {
            const Undo & child = i ? *pending[i - 1] : *s;
            incremental(pos, c, pending[i]->accumulator, child.accumulator, child, true);
            pending[i]->accumulator.computed_accumulation[c] = true;
            pending[i]->accumulator.computed_score = false;
        }
    }

    s->accumulator.computed_accumulation[c] = true;
    s->accumulator.computed_score = false;
}

template <std::int32_t OutputDimensions, std::int32_t InputDimensions>
Layer<OutputDimensions, InputDimensions>::Layer(std::istream & s) {
    memset(biases, 0, sizeof(biases));
    memset(weights, 0, sizeof(weights));

    s.read(reinterpret_cast<char*>(biases), sizeof(biases));
    s.read(reinterpret_cast<char*>(weights), sizeof(weights));

#if defined(USE_AVX2)
    if constexpr (SparseInput) {

        //
        // [output][input] -> [input / 4][output][input % 4]: one 4-byte chunk of the input meets the
        // weights of every output in a single 64-byte row, which the sparse propagation below relies on
        //

        std::vector<std::int8_t> rowMajor(weights, weights + OutputDimensions * InputDimensions);

        for (std::int32_t o = 0; o < OutputDimensions; ++o)
            for (std::int32_t i = 0; i < InputDimensions; ++i)
                weights[(i / 4) * OutputDimensions * 4 + o * 4 + i % 4] = rowMajor[o * InputDimensions + i];
    }
#endif
}

#if defined(USE_AVX2)

//
// mask to the positions of its set bits, padded to eight entries
//

alignas(CACHE_LINE) static const std::array<std::array<std::uint16_t, 8>, 256> NonZeroLookup = []() {
    std::array<std::array<std::uint16_t, 8>, 256> table{};

    for (unsigned mask = 0; mask < 256; ++mask)
        for (unsigned bit = 0, k = 0; bit < 8; ++bit)
            if (mask & (1u << bit))
                table[mask][k++] = static_cast<std::uint16_t>(bit);

    return table;
}();

//
// Writes the indexes of the 4-byte input chunks that are not zero and returns how many there are.
//

static inline std::uint32_t findNonZeroChunks(const std::uint8_t * input, std::uint16_t * out, std::uint32_t chunks) {

    std::uint32_t count = 0;
    __m128i       base  = _mm_setzero_si128();
    const __m128i step  = _mm_set1_epi16(8);

    auto append = [&](unsigned mask) {
        const __m128i offsets = _mm_load_si128(reinterpret_cast<const __m128i*>(NonZeroLookup[mask].data()));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(out + count), _mm_add_epi16(base, offsets));
        count += _mm_popcnt_u32(mask);
        base   = _mm_add_epi16(base, step);
    };

#if defined(USE_AVX512)
    for (std::uint32_t i = 0; i < chunks; i += 16) {
        const __m512i  v    = _mm512_load_si512(input + 4 * i);
        const unsigned mask = _mm512_test_epi32_mask(v, v);

        append(mask & 0xFF);
        append(mask >> 8);
    }
#else
    for (std::uint32_t i = 0; i < chunks; i += 8) {
        const __m256i v = _mm256_load_si256(reinterpret_cast<const __m256i*>(input + 4 * i));
        append(~unsigned(_mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpeq_epi32(v, _mm256_setzero_si256())))) & 0xFF);
    }
#endif

    return count;
}

#endif

template <std::int32_t OutputDimensions, std::int32_t InputDimensions>
inline std::int32_t * Layer<OutputDimensions, InputDimensions>::propagate(std::uint8_t * features, char * outBuffer) {

    auto output = reinterpret_cast<std::int32_t*>(outBuffer);

#if defined(USE_AVX2)
    if constexpr (SparseInput) {

        //
        // Most 4-byte chunks of the transformed features are zero, and a zero chunk adds nothing to any
        // output, so only the others are visited, each with the one row of weights its outputs share
        //

        static_assert(OutputDimensions == 16 && InputDimensions % 64 == 0);

        alignas(CACHE_LINE) std::uint16_t nonZero[InputDimensions / 4 + 8];
        const std::uint32_t count = findNonZeroChunks(features, nonZero, InputDimensions / 4);
        const auto chunks = reinterpret_cast<const std::int32_t*>(features);

#if defined(USE_AVX512)
        const auto rows = reinterpret_cast<const __m512i*>(weights);

        __m512i s0 = _mm512_setzero_si512();
        __m512i s1 = _mm512_setzero_si512();
        __m512i s2 = _mm512_setzero_si512();
        __m512i s3 = _mm512_setzero_si512();

        std::uint32_t k = 0;

        for (; k + 4 <= count; k += 4) {
            s0 = affine_acc_512(s0, _mm512_set1_epi32(chunks[nonZero[k + 0]]), _mm512_load_si512(&rows[nonZero[k + 0]]));
            s1 = affine_acc_512(s1, _mm512_set1_epi32(chunks[nonZero[k + 1]]), _mm512_load_si512(&rows[nonZero[k + 1]]));
            s2 = affine_acc_512(s2, _mm512_set1_epi32(chunks[nonZero[k + 2]]), _mm512_load_si512(&rows[nonZero[k + 2]]));
            s3 = affine_acc_512(s3, _mm512_set1_epi32(chunks[nonZero[k + 3]]), _mm512_load_si512(&rows[nonZero[k + 3]]));
        }

        for (; k < count; ++k)
            s0 = affine_acc_512(s0, _mm512_set1_epi32(chunks[nonZero[k]]), _mm512_load_si512(&rows[nonZero[k]]));

        const __m512i sum = _mm512_add_epi32(_mm512_add_epi32(s0, s1), _mm512_add_epi32(s2, s3));
        _mm512_storeu_si512(output, _mm512_add_epi32(sum, _mm512_loadu_si512(biases)));
#else
        const auto rows = reinterpret_cast<const __m256i*>(weights);

        __m256i s0lo = _mm256_setzero_si256(), s0hi = _mm256_setzero_si256();
        __m256i s1lo = _mm256_setzero_si256(), s1hi = _mm256_setzero_si256();

        std::uint32_t k = 0;

        for (; k + 2 <= count; k += 2) {
            const __m256i in0 = _mm256_set1_epi32(chunks[nonZero[k + 0]]);
            const __m256i in1 = _mm256_set1_epi32(chunks[nonZero[k + 1]]);

            s0lo = affine_acc_256(s0lo, in0, _mm256_load_si256(&rows[2 * nonZero[k + 0] + 0]));
            s0hi = affine_acc_256(s0hi, in0, _mm256_load_si256(&rows[2 * nonZero[k + 0] + 1]));
            s1lo = affine_acc_256(s1lo, in1, _mm256_load_si256(&rows[2 * nonZero[k + 1] + 0]));
            s1hi = affine_acc_256(s1hi, in1, _mm256_load_si256(&rows[2 * nonZero[k + 1] + 1]));
        }

        if (k < count) {
            const __m256i in0 = _mm256_set1_epi32(chunks[nonZero[k]]);

            s0lo = affine_acc_256(s0lo, in0, _mm256_load_si256(&rows[2 * nonZero[k] + 0]));
            s0hi = affine_acc_256(s0hi, in0, _mm256_load_si256(&rows[2 * nonZero[k] + 1]));
        }

        const auto bias = reinterpret_cast<const __m256i*>(biases);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(&output[0]), _mm256_add_epi32(_mm256_add_epi32(s0lo, s1lo), _mm256_loadu_si256(&bias[0])));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(&output[8]), _mm256_add_epi32(_mm256_add_epi32(s0hi, s1hi), _mm256_loadu_si256(&bias[1])));
#endif
        return output;
    }
#endif

#if defined(USE_AVX512)
    if constexpr (InputDimensions % (SIMD_WIDTH * 2) == 0) {
        const std::uint32_t chunks = InputDimensions / (SIMD_WIDTH * 2);
        const auto input_vector = reinterpret_cast<const __m512i*>(features);

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
                    s0 = affine_acc_512(s0, inp, _mm512_loadu_si512(&r0[j]));
                    s1 = affine_acc_512(s1, inp, _mm512_loadu_si512(&r1[j]));
                    s2 = affine_acc_512(s2, inp, _mm512_loadu_si512(&r2[j]));
                    s3 = affine_acc_512(s3, inp, _mm512_loadu_si512(&r3[j]));
                }
                const __m128i bias = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&biases[i]));
                _mm_storeu_si128(reinterpret_cast<__m128i*>(&output[i]), m512_haddx4(s0, s1, s2, s3, bias));
            }
            return output;
        }

        for (std::uint32_t i = 0; i < OutputDimensions; ++i) {
            __m512i sum = _mm512_setzero_si512();
            const auto row = reinterpret_cast<const __m512i*>(&weights[i * InputDimensions]);
            for (std::uint32_t j = 0; j < chunks; ++j)
                sum = affine_acc_512(sum, _mm512_loadu_si512(&input_vector[j]), _mm512_loadu_si512(&row[j]));
            output[i] = _mm512_reduce_add_epi32(sum) + biases[i];
        }
        return output;
    }
#endif

#if defined(USE_AVX2)
    std::uint32_t chunks = InputDimensions / SIMD_WIDTH;
    const auto input_vector = reinterpret_cast<const __m256i*>(features);
#endif

#if defined(USE_AVX2)
    // 4-way kernel blocking: process 4 output neurons simultaneously.
    // 4 independent accumulator chains allow better ILP vs. one serial chain per output.
    // Threshold is one full SIMD chunk so the small 32x32 hidden layer also takes this path,
    // where batching the four reductions is a bigger win than the dot-product itself.
    if constexpr (OutputDimensions % 4 == 0 && InputDimensions >= 32) {
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
                s0 = affine_acc_256(s0, inp, _mm256_load_si256(&r0[j]));
                s1 = affine_acc_256(s1, inp, _mm256_load_si256(&r1[j]));
                s2 = affine_acc_256(s2, inp, _mm256_load_si256(&r2[j]));
                s3 = affine_acc_256(s3, inp, _mm256_load_si256(&r3[j]));
            }
            const __m128i bias = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&biases[i]));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&output[i]), m256_haddx4(s0, s1, s2, s3, bias));
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
            sum = affine_acc_256(sum, _mm256_loadA_si256(&input_vector[j]), _mm256_load_si256(&row[j]));
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
#endif // PURE_HCE