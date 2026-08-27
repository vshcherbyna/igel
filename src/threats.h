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

#ifndef THREATS_H
#define THREATS_H

#include <cstdint>

#include "types.h"

class Position;

const std::uint32_t THREAT_HASH_VALUE = 0x8f234cb8u;
const std::uint32_t THREAT_DIMENSIONS = 60144;

class ThreatList
{
public:
    enum { MAX_LENGTH = 320 };

    void clear() { m_size = 0; }
    size_t size() const { return m_size; }
    std::uint16_t operator[](size_t i) const { return m_data[i]; }

    void add(std::uint32_t index) {
        assert(m_size < MAX_LENGTH);
        m_data[m_size] = static_cast<std::uint16_t>(index);
        m_size += (index < THREAT_DIMENSIONS);
    }

private:
    std::uint16_t m_data[MAX_LENGTH];
    size_t m_size = 0;
};

std::uint32_t makeThreatIndex(COLOR side, Piece attacker, Square from, Square to, Piece attacked, Square ksq);

void getActiveThreatIndexes(const Position & pos, COLOR side, ThreatList & active);

void getChangedThreatIndexes(COLOR side, Square ksq, const DirtyThreats & dirty, ThreatList & removed, ThreatList & added);

#endif // THREATS_H
