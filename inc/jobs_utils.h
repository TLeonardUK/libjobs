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

#include "jobs_defines.h"
#include "jobs_memory.h"

#include <stdint.h>
#include <chrono>
#include <atomic>
#include <shared_mutex>
#include <cassert>
#if defined(JOBS_PLATFORM_PS4)
// For _mm_pause intrinsic
#include <x86intrin.h>
#endif

/** @todo */
#define JOBS_MIN(x, y) ((x) < (y) ? (x) : (y))

/** @todo */
#define JOBS_MAX(x, y) ((x) > (y) ? (x) : (y))

/** @todo */
#if defined(JOBS_PLATFORM_SWITCH)
#define JOBS_YIELD() __asm__ __volatile__ ("yield")
#else
#define JOBS_YIELD() _mm_pause()
#endif

namespace jobs {
namespace internal {

/** @todo */
template <typename mutex_type>
struct optional_lock
{
public:
    optional_lock(mutex_type& mutex, bool should_lock)
        : m_mutex(mutex)
        , m_locked(should_lock)
    {
        if (m_locked)
        {
            m_mutex.lock();
        }
    }

    ~optional_lock()
    {
        if (m_locked)
        {
            m_mutex.unlock();
        }
    }

private:
    mutex_type& m_mutex;
    bool m_locked;

};

/** @todo */
template <typename mutex_type>
struct optional_shared_lock
{
public:
    optional_shared_lock(mutex_type& mutex, bool should_lock)
        : m_mutex(mutex)
        , m_locked(should_lock)
    {
        if (m_locked)
        {
            m_mutex.lock_shared();
        }
    }

    ~optional_shared_lock()
    {
        if (m_locked)
        {
            m_mutex.unlock_shared();
        }
    }

private:
    mutex_type& m_mutex;
    bool m_locked;

};

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
struct multiple_writer_single_reader_list
{
public:

    /** @todo */
    struct link
    {
        /** @todo */
        data_type value = nullptr;

        /** @todo */
        link* next = nullptr;

        /** @todo */
        link* prev = nullptr;
    };

    /** @todo */
    struct iterator
    {
    public:

        /** @todo */
        iterator()
        {
        }

        /** @todo */
        ~iterator()
        {
            if (m_locked)
            {
                //printf("Unlocked\n");
                m_owner->m_lock.unlock();
                m_locked = false;
            }
        }

        /** @todo */
        data_type value()
        {
            return m_link->value;
        }

        /** @todo */
        operator bool() const
        {
            return m_link != nullptr;
        }

        /** @todo */
        iterator& operator++(int)
        { 
            next();
            return *this;
        }

    private:

        /** @todo */
        bool start(multiple_writer_single_reader_list* owner, bool lock_required)
        {
            assert(m_locked == false);

            m_owner = owner;
            if (lock_required)
            {
                //printf("Locked\n");
                m_owner->m_lock.lock();
                m_locked = true;
            }
            m_link = m_owner->m_head.load();

            return (m_link != nullptr);
        }

        /** @todo */
        bool next()
        {
            m_link = m_link->next;
            return (m_link != nullptr);
        }

    private:

        friend struct multiple_writer_single_reader_list<data_type>;

        /** @todo */
        multiple_writer_single_reader_list* m_owner = nullptr;

        /** @todo */
        link* m_link = nullptr;

        /** @todo */
        bool m_locked = false;

    };

public:

    /** @todo */
    void add(link* value, bool lock_required = true)
    {
        optional_shared_lock<std::shared_timed_mutex> lock(m_lock, lock_required);

        while (true)
        {
            size_t old_change_index = m_change_index;
            size_t new_change_index = old_change_index + 1;

            if (m_uncommitted_change_index.compare_exchange_strong(old_change_index, new_change_index))
            {
                link* current_head = m_head.load();
                if (current_head != nullptr)
                {
                    current_head->prev = value;
                }
                value->next = current_head;
                value->prev = nullptr;
                m_head = value;

                m_change_index = m_uncommitted_change_index;
                break;
            }
            else
            {
                JOBS_YIELD();
            }
        }
    }

    /** @todo */
    void remove(link* value, bool lock_required = true)
    {
        optional_shared_lock<std::shared_timed_mutex> lock(m_lock, lock_required);

        while (true)
        {
            size_t old_change_index = m_change_index;
            size_t new_change_index = old_change_index + 1;

            if (m_uncommitted_change_index.compare_exchange_strong(old_change_index, new_change_index))
            {
                if (value->next != nullptr)
                {
                    value->next->prev = value->prev;
                }
                if (value->prev != nullptr)
                {
                    value->prev->next = value->next;
                }
                if (value == m_head.load())
                {
                    m_head = value->next;
                }

                m_change_index = m_uncommitted_change_index;
                break;
            }
            else
            {
                JOBS_YIELD();
            }
        }
    }

    /** @todo */
    bool iterate(iterator& iter, bool lock_required = true)
    {        
        return iter.start(this, lock_required);
    }

    /** @todo */
    std::shared_timed_mutex& get_mutex()
    {
        return m_lock;
    }

private:

    /** @todo */
    std::shared_timed_mutex m_lock;

    /** @todo */
    std::atomic<link*> m_head = nullptr;

    /** @todo */
    std::atomic<size_t> m_uncommitted_change_index{ 0 };

    /** @todo */
    size_t m_change_index = 0;
    
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
    result init(const memory_functions& memory_functions, int64_t capacity)
    {
        // implemented as an atomic circular buffer.
        m_buffer = (data_type*)memory_functions.user_alloc(sizeof(data_type) * capacity, alignof(data_type));
        m_memory_functions = memory_functions;

        m_head = 0;
        m_tail = 0;
        m_uncomitted_head = 0;
        m_uncomitted_tail = 0;

        m_capacity = capacity;

        return result::success;
    }

    /** @todo */
    result pop(data_type& result, bool can_block = false)
    {
        while (true)
        {
            int64_t old_tail = m_tail;
            int64_t new_tail = old_tail + 1;

            int64_t diff = (m_head - new_tail);
            if (diff < 0)
            {
                if (can_block)
                {
                    continue;
                }
                else
                {
                    return result::empty;
                }
            }

            if (m_uncomitted_tail.compare_exchange_strong(old_tail, new_tail))
            {
                result = m_buffer[old_tail % m_capacity];
                m_tail = m_uncomitted_tail;
                break;
            }
            else
            {
                JOBS_YIELD();
            }
        }

        return result::success;
    }

    /** @todo */
    result push(data_type value, bool can_block = true)
    {
        while (true)
        {
            int64_t old_head = m_head;
            int64_t new_head = old_head + 1;

            int64_t diff = (new_head - m_tail);
            if (diff > m_capacity)
            {
                if (can_block)
                {
                    continue;
                }
                else
                {
                    return result::maximum_exceeded;
                }
            }

            if (m_uncomitted_head.compare_exchange_strong(old_head, new_head))
            {
                m_buffer[old_head % m_capacity] = value;
                m_head = m_uncomitted_head;
                break;
            }
            else
            {
                JOBS_YIELD();
            }
        }

        return result::success;
    }

    /** @todo */
    size_t count()
    {
        return (size_t)(m_head - m_tail);
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
    int64_t m_head;

    /** @todo */
    int64_t m_tail;

    /** @todo */
    std::atomic<int64_t> m_uncomitted_head;

    /** @todo */
    std::atomic<int64_t> m_uncomitted_tail;

    /** @todo */
    int64_t m_capacity;

};

/** @todo */
template <typename data_type, int capacity>
struct fixed_queue
{
public:

    /** @todo */
    fixed_queue()
    {
        m_head = 0;
        m_tail = 0;
    }

    /** @todo */
    result pop(data_type& result)
    {
        int64_t old_tail = m_tail;
        int64_t new_tail = m_tail + 1;

        int64_t diff = (m_head - new_tail);
        if (diff < 0)
        {
            return result::empty;
        }

        result = m_buffer[old_tail % capacity];
        m_tail = new_tail;

        return result::success;
    }

    /** @todo */
    result push(data_type value)
    {
        int64_t old_head = m_head;
        int64_t new_head = old_head + 1;

        int64_t diff = (new_head - m_tail);
        if (diff > capacity)
        {
            return result::maximum_exceeded;
        }

        m_buffer[old_head % capacity] = value;
        m_head = new_head;

        return result::success;
    }

    /** @todo */
    size_t count()
    {
        return (size_t)(m_head - m_tail);
    }

    /** @todo */
    bool is_empty()
    {
        return (m_head == m_tail);
    }

private:

    /** @todo */
    data_type m_buffer[capacity];

    /** @todo */
    int64_t m_head;

    /** @todo */
    int64_t m_tail;

};

/**
 *  \brief Holds a fixed number of objects which can be allocated and freed.
 *
 *  Operations on this class are thread-safe.
 *
 *	\tparam data_type The type of object held in the pool. 
 */
template <typename data_type>
struct fixed_pool
{
public:

    /**
     *  \brief User-defined function used to initialize elements in a fixed_pool.
     *
     *  \param ptr Pointer to element to initialize.
     *  \param index Index of element inside pool.
     *
     *  \return Result of initialization, initialization will abort on first failed element.
     */
    typedef std::function<result(data_type* ptr, size_t index)> init_function;

public:

    /**
     * Constructor.
     *
     * \param memory_function User defined memory allocation overrides.
     */
    fixed_pool()
    {
    }

    /** Destructor. */
    ~fixed_pool()
    {
        if (m_objects != nullptr)
        {
            for (int i = 0; i < m_capacity; i++)
            {
                m_objects[i].~data_type();
            }
            m_memory_functions.user_free(m_objects);
            m_objects = nullptr;
        }
    }

    /** @todo */
    result init(const memory_functions& memory_functions, size_t capacity, const init_function& init_function)
    {
        m_memory_functions = memory_functions;
        m_capacity = capacity;

        // Alloc the object list.
        m_objects = static_cast<data_type*>(m_memory_functions.user_alloc(sizeof(data_type) * capacity, alignof(data_type)));
        if (m_objects == nullptr)
        {
            return result::out_of_memory;
        }

        // Initialize allocated objects.
        for (int i = 0; i < capacity; i++)
        {
            result res = init_function(m_objects + i, i);
            if (res != result::success)
            {
                return res;
            }
        }

        // Alloc and fill the free object list.
        result res = m_free_queue.init(memory_functions, capacity);
        if (res != result::success)
        {
            return res;
        }

        for (int i = 0; i < capacity; i++)
        {
            m_free_count++;
            m_free_queue.push(i);
        }

        return result::success;
    }

    /** @todo */
    result alloc(size_t& output)
    {
        m_free_count--;
        return m_free_queue.pop(output, true);
    }

    /** @todo */
    result free(data_type* object)
    {
        size_t index = (reinterpret_cast<char*>(object) - reinterpret_cast<char*>(m_objects)) / sizeof(data_type);

#if !defined(NDEBUG)
        memset(m_objects + index, 0xAB, sizeof(data_type));
#endif

        m_free_count++;
        return m_free_queue.push(index);
    }

    /** @todo */
    result free(size_t index)
    {
        return m_free_queue.push(index);
    }

    /** @todo */
    data_type* get_index(size_t index)
    {
        return &m_objects[index];
    }

    /** @todo */
    size_t count()
    {
        return m_capacity - m_free_queue.count();
    }

    /** @todo */
    size_t capacity()
    {
        return m_capacity;
    }

private:

    /** @todo */
    std::mutex m_access_mutex;

    /** @todo */
    memory_functions m_memory_functions;

    /** @todo */
    data_type* m_objects = nullptr;

    /** @todo */
    size_t m_capacity = 0;

    /** @todo */
    atomic_queue<size_t> m_free_queue;

    /** @todo */
    std::atomic<size_t> m_free_count{ 0 };
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