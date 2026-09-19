/*
*  Igel - a UCI chess playing engine derived from GreKo 2018.01
*
*  Copyright (C) 2018-2026 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
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

#ifndef TUNE_H
#define TUNE_H

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

//
//  Search parameters OpenBench is allowed to move with SPSA:
//
//      TUNABLE(member, uci name, default, min, max, c_end, r_end)
//
//  A tuning build (-DIGEL_TUNE) turns each of them into a UCI spin option and
//  teaches the engine to print the input block an SPSA workload expects. A
//  plain build folds the very same numbers back into constants, so a binary
//  built without the switch searches exactly as it did before.
//

#if defined(IGEL_TUNE)

class Tunable
{
public:
    Tunable(const char * name, int value, int minimum, int maximum, double cEnd, double rEnd);

    Tunable(const Tunable &) = delete;
    Tunable & operator = (const Tunable &) = delete;

    operator int() const { return m_value; }

    bool named(const std::string & name) const;
    void assign(int value);

    std::string option() const;
    std::string spsaInput() const;

private:
    const char * m_name;
    int m_value;
    int m_default;
    int m_min;
    int m_max;
    double m_cEnd;
    double m_rEnd;
};

namespace Tune
{
    inline std::vector<Tunable *> & parameters()
    {
        static std::vector<Tunable *> registry;
        return registry;
    }

    inline bool setOption(const std::string & name, const std::string & value)
    {
        for (auto parameter : parameters()) {
            if (parameter->named(name)) {
                parameter->assign(atoi(value.c_str()));
                return true;
            }
        }

        return false;
    }

    inline void printOptions()
    {
        for (auto parameter : parameters())
            std::cout << parameter->option() << std::endl;
    }

    inline void printSpsaInput()
    {
        for (auto parameter : parameters())
            std::cout << parameter->spsaInput() << std::endl;
    }
}

inline Tunable::Tunable(const char * name, int value, int minimum, int maximum, double cEnd, double rEnd) :
    m_name(name),
    m_value(value),
    m_default(value),
    m_min(minimum),
    m_max(maximum),
    m_cEnd(cEnd),
    m_rEnd(rEnd)
{
    Tune::parameters().push_back(this);
}

inline bool Tunable::named(const std::string & name) const
{
    if (name.length() != strlen(m_name))
        return false;

    for (size_t i = 0; i < name.length(); ++i) {
        if (tolower(static_cast<unsigned char>(name[i])) != tolower(static_cast<unsigned char>(m_name[i])))
            return false;
    }

    return true;
}

inline void Tunable::assign(int value)
{
    m_value = std::max(m_min, std::min(value, m_max));
}

inline std::string Tunable::option() const
{
    char buffer[256];

    snprintf(buffer, sizeof(buffer), "option name %s type spin default %d min %d max %d",
        m_name, m_default, m_min, m_max);

    return buffer;
}

inline std::string Tunable::spsaInput() const
{
    char buffer[256];

    snprintf(buffer, sizeof(buffer), "%s, int, %d, %d, %d, %.2f, %.4f",
        m_name, m_value, m_min, m_max, m_cEnd, m_rEnd);

    return buffer;
}

#define TUNABLE(member, uciName, value, minimum, maximum, cEnd, rEnd) \
    static inline Tunable member { uciName, value, minimum, maximum, cEnd, rEnd }

#else

namespace Tune
{
    inline bool setOption(const std::string &, const std::string &) { return false; }
    inline void printOptions() {}

    inline void printSpsaInput()
    {
        std::cout << "info string igel was built without tuning support, rebuild with 'make TUNE=1'" << std::endl;
    }
}

#define TUNABLE(member, uciName, value, minimum, maximum, cEnd, rEnd) \
    static constexpr int member = value

#endif

#endif
