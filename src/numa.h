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

#ifndef NUMA_H
#define NUMA_H

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#if defined(__linux__) && !defined(__ANDROID__)
#define NUMA_BINDING
#endif

//
//  On a machine with several NUMA nodes threads are bound to nodes, so that what they first write stays
//  close to them, and every node gets its own copy of the net
//

namespace Numa
{
    inline thread_local unsigned int threadNode = 0;

    bool setPolicy(const std::string & policy);

    void planThreads(unsigned int threads);

    bool threadsBound();
    unsigned int boundNodes();
    unsigned int nodeOfThread(unsigned int thread);
    void bindThisThread(unsigned int node);

    void runOnNode(unsigned int node, const std::function<void()> & task);

    std::string describe();

    std::vector<int> parseCpuList(const std::string & list);
    bool suggestsBinding(const std::vector<std::size_t> & nodeSizes, unsigned int threads);
    std::vector<unsigned int> distribute(const std::vector<std::size_t> & nodeSizes, unsigned int threads, unsigned int first);
}

#endif // NUMA_H
