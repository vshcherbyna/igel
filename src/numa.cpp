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

#include "numa.h"

#include <algorithm>
#include <cstdlib>
#include <limits>

#if defined(NUMA_BINDING)
#include <fstream>
#include <sched.h>
#include <thread>
#endif

std::vector<int> Numa::parseCpuList(const std::string & list)
{
    std::vector<int> cpus;
    const char * text = list.c_str();

    while (*text) {
        char * end;
        const long first = std::strtol(text, &end, 10);

        if (end == text || first < 0)
            return {};

        long last = first;
        text = end;

        if (*text == '-') {
            last = std::strtol(text + 1, &end, 10);

            if (end == text + 1 || last < first || last > 65535)
                return {};

            text = end;
        }

        for (long cpu = first; cpu <= last; ++cpu)
            cpus.push_back(static_cast<int>(cpu));

        if (*text == ',')
            ++text;
        else if (*text)
            return {};
    }

    return cpus;
}

//
//  Bind only when the threads cannot stay inside one node, or when there are enough of them to spread evenly
//

bool Numa::suggestsBinding(const std::vector<std::size_t> & nodeSizes, unsigned int threads)
{
    if (nodeSizes.size() < 2 || threads < 2)
        return false;

    const std::size_t largest = *std::max_element(nodeSizes.begin(), nodeSizes.end());
    const std::size_t notSmall = std::count_if(nodeSizes.begin(), nodeSizes.end(), [largest](std::size_t size) {
        return static_cast<double>(size) / static_cast<double>(largest) > 0.6;
    });

    return threads > largest / 2 || threads >= notSmall * 4;
}

//
//  Every thread goes to the node that it fills the least, thread 0 to the given node
//

std::vector<unsigned int> Numa::distribute(const std::vector<std::size_t> & nodeSizes, unsigned int threads, unsigned int first)
{
    std::vector<unsigned int> nodes;
    std::vector<std::size_t> used(nodeSizes.size(), 0);

    for (unsigned int thread = 0; thread < threads; ++thread) {
        unsigned int best = first;
        double bestFill = std::numeric_limits<double>::max();

        for (unsigned int node = 0; thread > 0 && node < nodeSizes.size(); ++node) {
            const double fill = static_cast<double>(used[node] + 1) / static_cast<double>(nodeSizes[node]);

            if (fill < bestFill) {
                best = node;
                bestFill = fill;
            }
        }

        nodes.push_back(best);
        ++used[best];
    }

    return nodes;
}

#if defined(NUMA_BINDING)

static std::string s_policy = "auto";
static bool s_nodesRead = false;
static cpu_set_t s_allowed;
static std::vector<cpu_set_t> s_nodes;
static std::vector<unsigned int> s_plan; // the node of each thread, empty when threads are not bound
static thread_local bool s_restricted = false;

static bool isSystemPolicy(const std::string & policy) {
    return policy == "auto" || policy == "system" || policy == "none";
}

static bool addNode(std::vector<cpu_set_t> & nodes, const std::string & cpus)
{
    cpu_set_t node;
    CPU_ZERO(&node);

    for (int cpu : Numa::parseCpuList(cpus))
        if (CPU_ISSET(cpu, &s_allowed))
            CPU_SET(cpu, &node);

    if (CPU_COUNT(&node) == 0)
        return false;

    nodes.push_back(node);
    return true;
}

bool Numa::setPolicy(const std::string & policy)
{
    // what the process was started with, as this thread is never bound
    if (sched_getaffinity(0, sizeof(s_allowed), &s_allowed))
        CPU_ZERO(&s_allowed);

    std::vector<cpu_set_t> nodes;

    if (isSystemPolicy(policy)) {
        std::ifstream online("/sys/devices/system/node/online");
        std::string line;

        if (std::getline(online, line))
            for (int id : parseCpuList(line)) {
                std::ifstream cpus("/sys/devices/system/node/node" + std::to_string(id) + "/cpulist");

                if (std::getline(cpus, line))
                    addNode(nodes, line); // a node without cpus is memory only, nothing can be bound to it
            }
    }
    else {
        std::string::size_type begin = 0, end;

        do {
            end = policy.find(':', begin);

            if (!addNode(nodes, policy.substr(begin, end == std::string::npos ? end : end - begin)))
                return false;

            begin = end + 1;
        } while (end != std::string::npos);
    }

    s_policy = policy;
    s_nodes = std::move(nodes);
    s_nodesRead = true;

    return true;
}

void Numa::planThreads(unsigned int threads)
{
    if (!s_nodesRead)
        setPolicy(s_policy);

    std::vector<std::size_t> sizes;

    for (const auto & node : s_nodes)
        sizes.push_back(CPU_COUNT(&node));

    const bool bind = s_policy == "auto"   ? suggestsBinding(sizes, threads)
                    : s_policy == "system" ? sizes.size() > 1
                    : s_policy != "none";

    s_plan.clear();

    if (!bind)
        return;

    //
    //  the principal goes to the node of this thread, which built the principal Search and which
    //  creates the principal's thread, writing its thread locals
    //

    unsigned int first = 0;
    const int cpu = sched_getcpu();

    for (unsigned int node = 0; cpu >= 0 && node < s_nodes.size(); ++node)
        if (CPU_ISSET(cpu, &s_nodes[node]))
            first = node;

    s_plan = distribute(sizes, threads, first);
}

unsigned int Numa::boundNodes()
{
    return s_plan.empty() ? 1 : static_cast<unsigned int>(s_nodes.size());
}

unsigned int Numa::nodeOfThread(unsigned int thread)
{
    return s_plan.empty() ? 0 : s_plan[thread % s_plan.size()];
}

void Numa::bindThisThread(unsigned int node)
{
    if (!s_plan.empty()) {
        threadNode = node;
        s_restricted = sched_setaffinity(0, sizeof(cpu_set_t), &s_nodes[node]) == 0;
    }
    else {
        threadNode = 0;

        if (s_restricted) {
            sched_setaffinity(0, sizeof(cpu_set_t), &s_allowed);
            s_restricted = false;
        }
    }
}

void Numa::runOnNode(unsigned int node, const std::function<void()> & task)
{
    if (s_plan.empty()) {
        task();
        return;
    }

    std::thread([node, &task]() {
        bindThisThread(node);
        task();
    }).join();
}

std::string Numa::describe()
{
    if (s_plan.empty())
        return "";

    std::vector<unsigned int> threads(s_nodes.size(), 0);

    for (unsigned int node : s_plan)
        ++threads[node];

    std::string text = "threads bound to NUMA nodes:";

    for (unsigned int count : threads)
        text += " " + std::to_string(count);

    return text;
}

#else

bool Numa::setPolicy(const std::string & policy) { return policy == "auto" || policy == "none"; }
void Numa::planThreads(unsigned int) {}
unsigned int Numa::boundNodes() { return 1; }
unsigned int Numa::nodeOfThread(unsigned int) { return 0; }
void Numa::bindThisThread(unsigned int) {}
void Numa::runOnNode(unsigned int, const std::function<void()> & task) { task(); }
std::string Numa::describe() { return ""; }

#endif
