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
#include <cmath>

#if defined(JOBS_PLATFORM_PS4)
// For _mm_pause intrinsic
#include <x86intrin.h>
#endif

/**
* \brief Returns minimum of two numbers.
*
* \param x First number.
* \param y Second number.
*
* \return Minimum of x and y.
*/
#define JOBS_MIN(x, y) ((x) < (y) ? (x) : (y))

/**
* \brief Returns maximum of two numbers.
*
* \param x First number.
* \param y Second number.
*
* \return Maximum of x and y.
*/
#define JOBS_MAX(x, y) ((x) > (y) ? (x) : (y))

namespace jobs {
namespace internal {
    
/**
* \brief Prints a meessage to debug output in a cross-platform way.
*
* \param format printf format message.
* \param ... Arguments to format message with.
*/
void debug_print(const char* format, ...);

/**
* \brief Gets the index of the highest bit set in a given value.
*
* \param value Value to search.
*
* \return Index of highest bit set in value.
*/
template <typename data_type>
data_type get_first_set_bit_pos(data_type value)
{
    return log2(value & -value) + 1;
}

/**
* \brief Performs RAII scope locking of a mutex. Similar to scope_lock except
*        mutex can be optionally acquired based on parameter passed to constructor.
*
* \tparam mutex_type Class of mutex this lock should operate on.
*/
template <typename mutex_type>
struct optional_lock
{
public:

    /**
    * \brief Constructor
    *
    * \param mutex Mutex that we should acquire a lock on.
    * \param should_lock If we should acquire a lock.
    */
    optional_lock(mutex_type& mutex, bool should_lock = true)
        : m_mutex(mutex)
        , m_locked(should_lock)
    {
        if (m_locked)
        {
            m_mutex.lock();
        }
    }

    /** Destructor. */
    ~optional_lock()
    {
        if (m_locked)
        {
            m_mutex.unlock();
        }
    }

private:

    /** Mutex we are operating on. */
    mutex_type& m_mutex;

    /** True if we have acquired a lock on \ref m_mutex */
    bool m_locked;

};

/**
* \brief Performs RAII scope shared locking of a mutex. Similar to scope_lock except
*        mutex can be optionally acquired based on parameter passed to constructor.
*
* This is the same as optional_lock except it attempts to get a shared_lock (see std::shared_mutex).
*
* \tparam mutex_type Class of mutex this lock should operate on.
*/
template <typename mutex_type>
struct optional_shared_lock
{
public:

    /**
    * \brief Constructor
    *
    * \param mutex Mutex that we should acquire a lock on.
    * \param should_lock If we should acquire a lock.
    */
    optional_shared_lock(mutex_type& mutex, bool should_lock = true)
        : m_mutex(mutex)
        , m_locked(should_lock)
    {
        if (m_locked)
        {
            m_mutex.lock_shared();
        }
    }

    /** Destructor. */
    ~optional_shared_lock()
    {
        if (m_locked)
        {
            m_mutex.unlock_shared();
        }
    }

private:

    /** Mutex we are operating on. */
    mutex_type& m_mutex;

    /** True if we have acquired a lock on \ref m_mutex */
    bool m_locked;

};

/**
 *  \brief Utility class used to time the duration between two points in code.
 */
struct stopwatch
{
public:

    /** Starts measuring time. */
    void start();

    /** Stops measuring time. */
    void stop();

    /**
    * \brief Number of elapsed milliseconds between \ref start and \ref stop calls.
    *
    * If stop has not been called yet, this gives a running value.
    *
    * \return Number of elapsed milliseconds.
    */
    uint64_t get_elapsed_ms();

    /**
    * \brief Number of elapsed microseconds between \ref start and \ref stop calls.
    *
    * If stop has not been called yet, this gives a running value.
    *
    * \return Number of elapsed microseconds.
    */
    uint64_t get_elapsed_us();

private:

    /** Timestamp when \ref start was called. */
    std::chrono::high_resolution_clock::time_point m_start_time;

    /** Timestamp when \ref stop was called. */
    std::chrono::high_resolution_clock::time_point m_end_time;

    /** If \ref stop has been called and \ref m_end_time is valid. */
    bool m_has_end = false;

};

/**
 *  \brief Similar to a normal mutex except control is never passed to the 
 *         OS when mutex is contented, instead a spinwait is performed.
 */
struct spinwait_mutex
{
public:

    /** Acquires a lock on the mutex. */
    JOBS_FORCE_INLINE void lock()
    {
        while (true)
        {
            uint8_t old_value = 0;
            uint8_t new_value = 1;

            if (m_locked.compare_exchange_strong(old_value, new_value))
            {
                return;
            }

            JOBS_YIELD();
        }
    }

    /** Release a lock on the mutex. */
    JOBS_FORCE_INLINE void unlock()
    {
        m_locked = 0;
    }

    /** Acquires a shared lock on the mutex. */
    JOBS_FORCE_INLINE void lock_shared()
    {
        lock();
    }

    /** Release a shared lock on the mutex. */
    JOBS_FORCE_INLINE void unlock_shared()
    {
        unlock();
    }

private:

    /** Atomic value representing lock state. */
    std::atomic<uint8_t> m_locked{0};

}; 

/**
 * \brief Thread-safe list that supports having multiple writers but a single reader.
 *
 * This is primary used for things like wait-queues where multiple threads will want to add things to it,
 * but only a single-thread would want to read (and block writes during the read). 
 */
template <typename data_type>
struct multiple_writer_single_reader_list
{
public:

    /** Individual link within the list. */
    struct link
    {
        /** Value held by this link */
        data_type value = nullptr;

        /** Next link in list */
        link* next = nullptr;

        /** Previous link in list */
        link* prev = nullptr;
    };

    /** Provides functionality to iterate over list in a thread-safe manner. */
    struct iterator
    {
    public:

        /** Constructor */
        iterator()
        {
        }

        /** Destructor */
        ~iterator()
        {
            if (m_locked)
            {
                m_owner->m_lock.unlock();
                m_locked = false;
            }
        }

        /**
        * \brief Gets the value held by the current link iterator is at.
        *
        * \return Value of current link.
        */
        JOBS_FORCE_INLINE data_type value()
        {
            return m_link->value;
        }

        /**
        * \brief Conversion to bool operator.
        *
        * \return True if there are more values to iterate over.
        */
        JOBS_FORCE_INLINE operator bool() const
        {
            return m_link != nullptr;
        }

        /**
        * \brief Post increment operator. 
        *
        * Causes the iterator to move onto the next value in the list.
        *
        * \return Reference to this iterator.
        */
        JOBS_FORCE_INLINE iterator& operator++(int)
        { 
            next();
            return *this;
        }

        /**
        * \brief Removes the current link.
        *
        * \return True if there are more values to iterate over.
        */
        JOBS_FORCE_INLINE bool remove()
        {
            // Remove first.
            link* original = m_link;
            link* next = m_link->next;

            if (original->next != nullptr)
            {
                original->next->prev = original->prev;
            }
            if (original->prev != nullptr)
            {
                original->prev->next = original->next;
            }
            if (original == m_owner->m_head.load())
            {
                m_owner->m_head = original->next;
            }

            // Move onto next.
            m_link = next;
            return (m_link != nullptr);
        }

    private:

        /**
        * \brief Starts itererating from the beginning of the given list.
        *
        * \param owner List to iterate over.
        * \param lock_required If a read lock needs to be acquired on the list. If not its assumed
        *                      the lock has been acquired further up the callstack.
        *
        * \return True if there are more values to iterate over.
        */
        JOBS_FORCE_INLINE bool start(multiple_writer_single_reader_list* owner, bool lock_required)
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

        /**
        * \brief Moves to the next value in the list.
        *
        * \return True if there are more values to iterate over.
        */
        JOBS_FORCE_INLINE bool next()
        {
            m_link = m_link->next;
            return (m_link != nullptr);
        }

    private:

        friend struct multiple_writer_single_reader_list<data_type>;

        /** List being iterated over. */
        multiple_writer_single_reader_list* m_owner = nullptr;

        /** Current link in the list. */
        link* m_link = nullptr;

        /** If we have acquired a read lock for the list. */
        bool m_locked = false;

    };

public:

    /** Constructor. */
    multiple_writer_single_reader_list()
    {
    }

    /**
    * \brief Adds the given link to the list.
    *
    * \param value Link to add to the list.
    * \param lock_required If a write lock needs to be acquired on the list. If not its assumed
    *                      the lock has been acquired further up the callstack.
    */
    JOBS_FORCE_INLINE void add(link* value, bool lock_required = true)
    {
        optional_shared_lock<spinwait_mutex> lock(m_lock, lock_required);

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

    /**
    * \brief Removes the given link from the list.
    *
    * \param value Link to remove from the list.
    * \param lock_required If a write lock needs to be acquired on the list. If not its assumed
    *                      the lock has been acquired further up the callstack.
    */
    JOBS_FORCE_INLINE void remove(link* value, bool lock_required = true)
    {
        optional_shared_lock<spinwait_mutex> lock(m_lock, lock_required);

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

    /**
    * \brief Creates an iterator for use on this list.
    *
    * \param iter Reference to storage that will hold the created iterator.
    * \param lock_required If a read lock needs to be acquired on the list. If not its assumed
    *                      the lock has been acquired further up the callstack.
    */
    JOBS_FORCE_INLINE bool iterate(iterator& iter, bool lock_required = true)
    {        
        return iter.start(this, lock_required);
    }

    /**
    * \brief Gets the shared mutex used for read/write exclusion.
    *
    * \return Mutex used by list.
    */
    JOBS_FORCE_INLINE spinwait_mutex& get_mutex()
    {
        return m_lock;
    }

private:

    /** Mutex used for read/write control. */
    spinwait_mutex m_lock;

    /** First link in the list. */
    std::atomic<link*> m_head = nullptr;

    /** Holds a value equal to the next m_change_index that is going to be committed. Prevents multiple writers changing the list at the same time. */
    std::atomic<size_t> m_uncommitted_change_index{ 0 };

    /** Incrementing variable representing the number of times the list has changed, used in tandem with \ref m_uncommitted_change_index to prevent concurrent writes. */
    size_t m_change_index = 0;
    
};

/**
 * \brief Thread-safe lock-less queue. 
 *
 * This is implemented internally as a ring buffer.
 */
template <typename data_type>
struct atomic_queue
{
public:

    /** Destructor. */
    ~atomic_queue()
    {
        if (m_buffer != nullptr)
        {
            m_memory_functions.user_free(m_buffer);
            m_buffer = nullptr;
        }
    }

    /**
     * \brief Initializes this queue to the given capacity.
     *
     * The only memory allocated by this queue is during this function.
     *
     * \param memory_functions Functions to use for allocating and deallocating queue buffers.
     * \param capacity Maximum capacity of queue.
     *
     * \return Value indicating the success of this function.
     */
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

    /**
     * \brief Pops off the first value in the queue.
     *
     * \param result Reference to store poped value.
     * \param can_block If true and the queue is empty, this function will block until a value is 
     *                  available, otherwise \ref result::empty will be returned.
     *
     * \return Value indicating the success of this function.
     */
    JOBS_FORCE_INLINE result pop(data_type& result, bool can_block = false)
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

    /**
     * \brief Pushes a new value into the queue.
     *
     * \param value New value to push into the queue.
     * \param can_block If true and the queue is full, this function will block until space is
     *                  available, otherwise \ref result::maximum_exceeded will be returned.
     *
     * \return Value indicating the success of this function.
     */
    JOBS_FORCE_INLINE result push(data_type value, bool can_block = true)
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

    /**
     * \brief Pushes a number of items into the queue in a single operation.
     *
     * \param buffer Pointer to the first value to push into the queue.
     * \param stride Byte offset to add to buffer to get subsequent value.
     * \param count Number of values that need to be pushed into queue.
     * \param can_block If true and the queue is full, this function will block until space is
     *                  available, otherwise \ref result::maximum_exceeded will be returned.
     *
     * \return Value indicating the success of this function.
     */
    JOBS_FORCE_INLINE result push_batch(data_type* buffer, size_t stride, size_t count, bool can_block = true)
    {
        while (true)
        {
            int64_t old_head = m_head;
            int64_t new_head = old_head + count;

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
                for (size_t i = 0; i < count; i++)
                {
                    data_type* ptr = reinterpret_cast<data_type*>(reinterpret_cast<char*>(buffer) + (stride * i));
                    m_buffer[(old_head + i) % m_capacity] = *ptr;
                }

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

    /**
    * \brief Gets the number of items in the queue.
    *
    * \return Number of items in the queue.
    */ 
    JOBS_FORCE_INLINE size_t count()
    {
        return (size_t)(m_head - m_tail);
    }

    /**
    * \brief Gets if this queue is empty.
    *
    * \return True if queue is empty.
    */
    JOBS_FORCE_INLINE bool is_empty()
    {
        return (m_head == m_tail);
    }

private:

    /** Linear data buffer storing all values in queue. */
    data_type* m_buffer = nullptr;

    /** Memory functions used for memory allocation. */
    memory_functions m_memory_functions;

    /** Current write position in ring buffer. (Need to modulus to get buffer index). */
    int64_t m_head;

    /** Current read position in ring buffer. (Need to modulus to get buffer index). */
    int64_t m_tail;

    /** Current position that a write is being permitted in-progress on. */
    std::atomic<int64_t> m_uncomitted_head;

    /** Current position that a read is being permitted in-progress on. */
    std::atomic<int64_t> m_uncomitted_tail;

    /** Maximum number of items in queue. */
    int64_t m_capacity;

};

/**
 * \brief Fixed size, statically-allocated, thread-unsafe queue.
 *
 * This is implemented internally as a ring buffer.
 *
 * \tparam data_type Type of value held in queue.
 * \tparam capacity Maximum number of values queue can hold.
 */
template <typename data_type, int capacity>
struct fixed_queue
{
public:

    /** Constructor. */
    fixed_queue()
    {
        m_head = 0;
        m_tail = 0;
    }

    /**
     * \brief Pops off the first value in the queue.
     *
     * Returns \ref result::empty if no value is available.
     *
     * \param result Reference to store poped value.
     *
     * \return Value indicating the success of this function.
     */
    JOBS_FORCE_INLINE result pop(data_type& result) volatile
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

    /**
     * \brief Pushes a new value into the queue.
     *
     * Returns \ref result::maximum_exceeded if no space is available.
     *
     * \param value New value to push into the queue.
     *
     * \return Value indicating the success of this function.
     */
    JOBS_FORCE_INLINE result push(data_type value) volatile
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

    /**
    * \brief Gets the number of items in the queue.
    *
    * \return Number of items in the queue.
    */
    JOBS_FORCE_INLINE size_t count()
    {
        return (size_t)(m_head - m_tail);
    }

    /**
    * \brief Gets if this queue is empty.
    *
    * \return True if queue is empty.
    */
    JOBS_FORCE_INLINE  bool is_empty()
    {
        return (m_head == m_tail);
    }

private:

    /** Linear data buffer to store values. */
    data_type m_buffer[capacity];

    /** Current write position in ring buffer. (Need to modulus to get buffer index). */
    int64_t m_head;

    /** Current read position in ring buffer. (Need to modulus to get buffer index). */
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

    /**
     * \brief Initializes this pool to the given capacity.
     *
     * The only memory allocated by this queue is during this function.
     *
     * \param memory_functions Functions to use for allocating and deallocating pool buffers.
     * \param capacity Maximum capacity of pool.
     * \param init_function Function to call on each element in pool to initialize it.
     *
     * \return Value indicating the success of this function.
     */
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
            m_free_queue.push(i);
        }

        return result::success;
    }

    /**
     * \brief Allocates a new object from the pool.
     *
     * \param output Reference to store index of allocated object.
     *
     * \return Value indicating the success of this function.
     */
    JOBS_FORCE_INLINE result alloc(size_t& output)
    {
        return m_free_queue.pop(output, true);
    }

    /**
     * \brief Frees an object previously allocated with \ref alloc.
     *
     * \param object Object to be freed.
     *
     * \return Value indicating the success of this function.
     */
    JOBS_FORCE_INLINE result free(data_type* object)
    {
        size_t index = (reinterpret_cast<char*>(object) - reinterpret_cast<char*>(m_objects)) / sizeof(data_type);

#if defined(JOBS_DEBUG_BUILD)
        memset(m_objects + index, 0xAB, sizeof(data_type));
#endif

        return m_free_queue.push(index);
    }

    /**
     * \brief Frees an object previously allocated with \ref alloc.
     *
     * \param index Index of object within pool.
     *
     * \return Value indicating the success of this function.
     */
    JOBS_FORCE_INLINE result free(size_t index)
    {
        return m_free_queue.push(index);
    }

    /**
     * \brief Gets a point to a pool object based on it's index.
     *
     * \param index Index of object within pool.
     *
     * \return Pointer to object with the given index.
     */
    JOBS_FORCE_INLINE data_type* get_index(size_t index)
    {
        return &m_objects[index];
    }

    /**
    * \brief Gets the number of allocated objects in the pool.
    *
    * \return Number of allocated objects in the pool.
    */
    JOBS_FORCE_INLINE size_t count()
    {
        return m_capacity - m_free_queue.count();
    }

    /**
    * \brief Gets the maximum number of objects in the pool.
    *
    * \return Maximum number of objects in the pool.
    */
    JOBS_FORCE_INLINE size_t capacity()
    {
        return m_capacity;
    }

private:

    /** Memory functions used for memory allocation. */
    memory_functions m_memory_functions;

    /** Linear buffer of all objects held in this pool. */
    data_type* m_objects = nullptr;

    /** Maximum number of objects in the pool. */
    size_t m_capacity = 0;

    /** Atomic queue of all indexes of objects that are current free for allocation. */
    atomic_queue<size_t> m_free_queue;
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