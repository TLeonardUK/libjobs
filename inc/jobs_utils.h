/*
  libjobs - Simple coroutine based job scheduling.
  Copyright (C) 2019 Tim Leonard <me@timleonard.uk>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.
  
  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/**
 *  \file jobs_utils.h
 *
 *  Include header for general library utilities.
 */

#ifndef __JOBS_UTILS_H__
#define __JOBS_UTILS_H__

#include <stdint.h>
#include <chrono>
#include <atomic>

#include "jobs_memory.h"

namespace jobs {
namespace internal {

/**
 *  \brief Utility class used to time the duration between two points in code.
 */
struct stopwatch
{
public:

    /** @todo */
    void start();

    /** @todo */
    void stop();

    /** @todo */
    uint64_t get_elapsed_ms();

    /** @todo */
    uint64_t get_elapsed_us();

private:

    /** @todo */
    std::chrono::high_resolution_clock::time_point m_start_time;

    /** @todo */
    std::chrono::high_resolution_clock::time_point m_end_time;

    /** @todo */
    bool m_has_end = false;

};

/** @todo */
template <typename data_type>
struct atomic_queue
{
public:

    /** @todo */
    ~atomic_queue()
    {
        if (m_buffer != nullptr)
        {
            m_memory_functions.user_free(m_buffer);
            m_buffer = nullptr;
        }
    }

    /** @todo */
    result init(const memory_functions& memory_functions, size_t capacity)
    {
        // implemented as an atomic circular buffer.
        m_buffer = (data_type*)memory_functions.user_alloc(sizeof(data_type) * capacity);
        m_memory_functions = memory_functions;

        m_head = 0;
        m_tail = 0;
        m_uncomitted_head = 0;
        m_uncomitted_tail = 0;

        m_capacity = capacity;

        return result::success;
    }

    /** @todo */
    result pop(data_type& result)
    {
        while (true)
        {
            size_t old_tail = m_tail;
            size_t new_tail = old_tail + 1;

            size_t diff = (m_head - new_tail);
            if (diff < 0 || diff > m_capacity)
            {
                return result::empty;
            }

            if (m_uncomitted_tail.compare_exchange_strong(old_tail, new_tail))
            {
                result = m_buffer[old_tail % m_capacity];
                m_tail = m_uncomitted_tail;
                break;
            }
            else
            {
                _mm_pause();
            }
        }

        return result::success;
    }

    /** @todo */
    result push(data_type value)
    {
        while (true)
        {
            size_t old_head = m_head;
            size_t new_head = old_head + 1;

            size_t diff = (new_head - m_tail);
            if (diff < 0 || diff > m_capacity)
            {
                continue;
            }

            if (m_uncomitted_head.compare_exchange_strong(old_head, new_head))
            {
                m_buffer[old_head % m_capacity] = value;
                m_head = m_uncomitted_head;
                break;
            }
            else
            {
                _mm_pause();
            }
        }

        return result::success;
    }

    /** @todo */
    size_t count()
    {
        return (m_head - m_tail);
    }

    /** @todo */
    bool is_empty()
    {
        return (m_head == m_tail);
    }

private:

    /** @todo */
    data_type* m_buffer = nullptr;

    /** @todo */
    memory_functions m_memory_functions;

    /** @todo */
    size_t m_head;

    /** @todo */
    size_t m_tail;

    /** @todo */
    std::atomic<size_t> m_uncomitted_head;

    /** @todo */
    std::atomic<size_t> m_uncomitted_tail;

    /** @todo */
    size_t m_capacity;

};

}; /* namespace internal */

/**
 *  \brief Represents a period of time used as a timeout for a blocking function.
 */
struct timeout
{
public:

    /** Duration of this timeout in milliseconds. */
    uint64_t duration;

    /**
     *  \brief Default constructor
     */
    timeout()
        : duration(0)
    {
    }

    /**
     *  \brief Constructor
     *
     *  \param inDuration Duration in milliseconds of this timeout.
     */
    timeout(uint64_t inDuration)
        : duration(inDuration)
    {
    }

    /**
     *  \brief Returns true if this timeout is infinite.
     *
     *  \return true if an infinite timeout.
     */
    bool is_infinite()
    {
        return duration == infinite.duration;
    }

    /** Represents an infinite, non-ending timeout. */
    static const timeout infinite;

};

}; /* namespace jobs */

#endif /* __JOBS_ENUM_H__ */