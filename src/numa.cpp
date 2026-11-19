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

//
//  Neighbouring L3 caches are merged
//

std::vector<std::vector<int>> Numa::groupCaches(std::vector<std::vector<int>> caches, std::size_t limit) {
    bool merged;

    do {
        merged = false;

        for (std::size_t i = 0; i + 1 < caches.size(); ++i)
            if (caches[i].size() + caches[i + 1].size() <= limit) {
                caches[i].insert(caches[i].end(), caches[i + 1].begin(), caches[i + 1].end());
                caches.erase(caches.begin() + i + 1);
                merged = true;
            }
    } while (merged);

    return caches;
}

#if defined(NUMA_BINDING)

static constexpr std::size_t s_groupCpus = 32;
static std::string s_policy = "auto";
static bool s_nodesRead = false;
static cpu_set_t s_allowed;
static std::vector<cpu_set_t> s_nodes;
static std::vector<unsigned int> s_netOf; // the net copy of each node
static unsigned int s_netCount = 1;
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

//
//  The cpus of a node that share each L3 cache, nothing when the system does not tell
//

static std::vector<std::vector<int>> readCaches(const cpu_set_t & node) {
    std::vector<std::vector<int>> caches;
    cpu_set_t seen;
    CPU_ZERO(&seen);

    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
        if (!CPU_ISSET(cpu, &node) || CPU_ISSET(cpu, &seen))
            continue;

        std::string list;

        for (int index = 0; list.empty(); ++index) {
            const std::string path = "/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/cache/index" + std::to_string(index) + "/";
            std::ifstream level(path + "level");
            std::string value;

            if (!level)
                return {};

            if (std::getline(level, value) && value == "3") {
                std::ifstream shared(path + "shared_cpu_list");

                if (!std::getline(shared, list) || list.empty())
                    return {};
            }
        }

        std::vector<int> cache;

        for (int other : Numa::parseCpuList(list))
            if (other < CPU_SETSIZE) {
                CPU_SET(other, &seen);

                if (CPU_ISSET(other, &node))
                    cache.push_back(other);
            }

        if (cache.empty())
            return {};

        caches.push_back(std::move(cache));
    }

    return caches;
}

bool Numa::setPolicy(const std::string & policy) {
    // what the process was started with, as this thread is never bound
    if (sched_getaffinity(0, sizeof(s_allowed), &s_allowed))
        CPU_ZERO(&s_allowed);

    std::vector<cpu_set_t> nodes;
    std::vector<unsigned int> netOf;
    unsigned int netCount;

    if (isSystemPolicy(policy)) {
        std::vector<cpu_set_t> numaNodes;
        std::ifstream online("/sys/devices/system/node/online");
        std::string line;

        if (std::getline(online, line))
            for (int id : parseCpuList(line)) {
                std::ifstream cpus("/sys/devices/system/node/node" + std::to_string(id) + "/cpulist");

                if (std::getline(cpus, line))
                    addNode(numaNodes, line); // a node without cpus is memory only, nothing can be bound to it
            }

        for (unsigned int numaNode = 0; numaNode < numaNodes.size(); ++numaNode) {
            auto caches = groupCaches(readCaches(numaNodes[numaNode]), s_groupCpus);

            if (caches.size() < 2) {
                nodes.push_back(numaNodes[numaNode]);
                netOf.push_back(numaNode);
                continue;
            }

            for (const auto & cache : caches) {
                cpu_set_t group;
                CPU_ZERO(&group);

                for (int cpu : cache)
                    CPU_SET(cpu, &group);

                nodes.push_back(group);
                netOf.push_back(numaNode);
            }
        }

        netCount = static_cast<unsigned int>(numaNodes.size());
    }
    else {
        std::string::size_type begin = 0, end;

        do {
            end = policy.find(':', begin);

            if (!addNode(nodes, policy.substr(begin, end == std::string::npos ? end : end - begin)))
                return false;

            begin = end + 1;
        } while (end != std::string::npos);

        for (unsigned int node = 0; node < nodes.size(); ++node)
            netOf.push_back(node);

        netCount = static_cast<unsigned int>(nodes.size());
    }

    s_policy = policy;
    s_nodes = std::move(nodes);
    s_netOf = std::move(netOf);
    s_netCount = netCount;
    s_nodesRead = true;

    return true;
}

std::string Numa::policy()
{
    return s_policy;
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

bool Numa::threadsBound()
{
    return !s_plan.empty();
}

unsigned int Numa::boundNodes()
{
    return s_plan.empty() ? 1 : static_cast<unsigned int>(s_nodes.size());
}

unsigned int Numa::netCopies()
{
    return s_plan.empty() ? 1 : s_netCount;
}

unsigned int Numa::netOfNode(unsigned int node)
{
    return s_plan.empty() ? 0 : s_netOf[node];
}

unsigned int Numa::nodeOfThread(unsigned int thread)
{
    return s_plan.empty() ? 0 : s_plan[thread % s_plan.size()];
}

void Numa::bindThisThread(unsigned int node)
{
    if (!s_plan.empty()) {
        threadNode = s_netOf[node];
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

    std::vector<unsigned int> threads(s_netCount, 0), groups(s_nodes.size(), 0);

    for (unsigned int node : s_plan) {
        ++threads[s_netOf[node]];
        ++groups[node];
    }

    std::string text = "threads bound to NUMA nodes:";

    for (unsigned int count : threads)
        text += " " + std::to_string(count);

    if (groups.size() != threads.size()) {
        text += ", in L3 groups:";

        for (unsigned int count : groups)
            text += " " + std::to_string(count);
    }

    return text;
}

#else

static std::string s_policy = "auto";

bool Numa::setPolicy(const std::string & policy)
{
    if (policy != "auto" && policy != "none")
        return false;

    s_policy = policy;
    return true;
}

std::string Numa::policy() { return s_policy; }
void Numa::planThreads(unsigned int) {}
bool Numa::threadsBound() { return false; }
unsigned int Numa::boundNodes() { return 1; }
unsigned int Numa::netCopies() { return 1; }
unsigned int Numa::netOfNode(unsigned int) { return 0; }
unsigned int Numa::nodeOfThread(unsigned int) { return 0; }
void Numa::bindThisThread(unsigned int) {}
void Numa::runOnNode(unsigned int, const std::function<void()> & task) { task(); }
std::string Numa::describe() { return ""; }

#endif
