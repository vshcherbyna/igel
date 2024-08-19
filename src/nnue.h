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

#ifndef NNUE_H
#define NNUE_H

#include <string>
#include <memory>
#include <cstdint>
#include <immintrin.h>

#include "types.h"

class Position;

const EVAL VAL_P = 100;
const EVAL VAL_N = 310;
const EVAL VAL_B = 330;
const EVAL VAL_R = 500;
const EVAL VAL_Q = 1000;
const EVAL VAL_K = 20000;

#define CACHE_LINE       64
#define PSQT_BUCKETS     8
#define LAYERED_NETWORKS 8
#define WEIGHTS_SCALE    16
#define PSQT_THRESHOLD   1400
#define SIMD_WIDTH       32
#define TILE_HEIGHT      256
#define NUM_REGS         16
#define NUM_PSQT_REGS    1
#define PSQT_TILE_HEIGHT 8
#define MAX_CHUNK_SIZE   32

class Transformer
{
public:
    Transformer(std::istream & s);

public:
    std::int32_t transform(Position & pos, std::uint8_t * outBuffer, const std::size_t bucket);
    inline void refresh(Position & pos);
    inline void incremental(Position & pos);

public:
    static constexpr int HalfDimensions  = 1024;
    static constexpr int InputDimensions = 22528;

public:
    alignas(CACHE_LINE) int16_t biases[HalfDimensions];
    alignas(CACHE_LINE) int16_t weights[HalfDimensions * InputDimensions];
    alignas(CACHE_LINE) int32_t psqts[InputDimensions  * PSQT_BUCKETS];
};

template <std::int32_t WeightScaleBits, std::int32_t InputDimensions> class ClippedReLU {

public:
    inline static std::uint8_t * propagate(std::int32_t * features, char * outBuffer);
    inline static std::uint8_t * propagateSqrt(std::int32_t* features, char* outBuffer);
};

template <std::int32_t OutputDimensions, std::int32_t InputDimensions> class Layer {
public:
    Layer(std::istream & s);

public:
    inline std::int32_t * propagate(std::uint8_t * features, char * outBuffer);

private:
    alignas(CACHE_LINE) std::int32_t biases[OutputDimensions];
    alignas(CACHE_LINE) std::int8_t  weights[OutputDimensions * InputDimensions];
};

class LayeredNetwork
{
public:
    LayeredNetwork(std::istream & s);

public:
    inline std::int32_t* propagate(std::uint8_t * features, char * outBuffer);

public:
    alignas(CACHE_LINE) Layer<16, 1024> inputLayer;
    alignas(CACHE_LINE) Layer<32, 32> hiddenLayer1;
    alignas(CACHE_LINE) Layer<1, 32> hiddenLayer2;
};

class Evaluator
{
public:
    static void initEval();
    static void initEval(std::istream & stream);
    EVAL evaluate(Position & pos);

private:
    int NnueEvaluate(Position & pos);

private:
    static std::unique_ptr<Transformer> m_transformer;
    static std::vector<std::unique_ptr<LayeredNetwork>> m_networks;

public:
    static constexpr int Tempo = 20;
};

#endif // NNUE_H
