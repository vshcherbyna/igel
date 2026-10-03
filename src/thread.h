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

#ifndef THREAD_H
#define THREAD_H

#include <thread>

//
//  std::thread cannot choose its stack size. A Windows thread reserves at least 1 MB, which a
//  search fits in, but musl gives a thread only 128 KB, so on posix systems a search thread
//  asks for the 8 MB that glibc gives by default
//

#if defined(_WIN32)

using SearchThread = std::thread;

#else

#include <functional>
#include <pthread.h>
#include <system_error>

class SearchThread
{
public:
    SearchThread() = default;

    template <class Worker>
    SearchThread(void (Worker::*routine)(), Worker * worker) {
        auto task = new std::function<void()>([routine, worker]() { (worker->*routine)(); });

        pthread_attr_t attributes;
        pthread_attr_init(&attributes);
        pthread_attr_setstacksize(&attributes, m_stackSize);

        auto error = pthread_create(&m_thread, &attributes, run, task);
        pthread_attr_destroy(&attributes);

        if (error) {
            delete task;
            throw std::system_error(error, std::generic_category());
        }

        m_joinable = true;
    }

    SearchThread(SearchThread && other) noexcept : m_thread(other.m_thread), m_joinable(other.m_joinable) {
        other.m_joinable = false;
    }

    SearchThread & operator=(SearchThread && other) noexcept {
        m_thread         = other.m_thread;
        m_joinable       = other.m_joinable;
        other.m_joinable = false;
        return *this;
    }

    bool joinable() const { return m_joinable; }

    void join() {
        pthread_join(m_thread, nullptr);
        m_joinable = false;
    }

private:
    static void * run(void * task) {
        auto routine = static_cast<std::function<void()> *>(task);
        (*routine)();
        delete routine;
        return nullptr;
    }

    static constexpr size_t m_stackSize = 8 * 1024 * 1024;

    pthread_t   m_thread {};
    bool        m_joinable = false;
};

#endif

#endif
