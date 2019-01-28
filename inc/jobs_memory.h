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
 *  \file jobs_memory.h
 *
 *  Include header for memory functionality.
 */

#ifndef __JOBS_MEMORY_H__
#define __JOBS_MEMORY_H__

#include "jobs_enums.h"

#include <functional>
#include <mutex>

namespace jobs {

/**
 *  \brief User-defined memory allocation function.
 *
 *  Function prototype that can be passed into a job scheduler through
 *  set_memory_functions to override the default malloc memory allocator.
 *
 *  \param size Size of block of memory to be allocated.
 *  \return Pointer to block of memory that was allocated, or nullptr on failure.
 */
typedef std::function<void*(size_t size)> memory_alloc_function;

/**
 *  \brief User-defined memory deallocation function.
 *
 *  Function prototype that can be passed into a job scheduler through
 *  set_memory_functions to override the default free memory deallocator.
 *
 *  \param ptr Pointer to memory to be deallocated.
 */
typedef std::function<void(void* ptr)> memory_free_function;

/**
 *  \brief Holds all overrided functions used for managing memory.
 */
struct memory_functions
{
	memory_alloc_function user_alloc = nullptr;
	memory_free_function user_free = nullptr;
};

namespace internal {

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
		if (m_free_object_indices != nullptr)
		{
			m_memory_functions.user_free(m_free_object_indices);
			m_free_object_indices = nullptr;
		}
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
		m_objects = static_cast<data_type*>(m_memory_functions.user_alloc(sizeof(data_type) * capacity));
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
		m_free_object_count = m_capacity;
		m_free_object_indices = static_cast<size_t*>(m_memory_functions.user_alloc(sizeof(size_t) * capacity));
		for (int i = 0; i < capacity; i++)
		{
			m_free_object_indices[i] = i;
		}

		return result::success;
	}

	/** @todo */
	result alloc(size_t& output)
	{
		// @todo: make this atomic
		std::lock_guard<std::mutex> lock(m_access_mutex);

		if (m_free_object_count == 0)
		{
			return result::out_of_objects;
		}

		size_t free_index = --m_free_object_count;

		output = m_free_object_indices[free_index];

		return result::success;
	}

	/** @todo */
	result free(size_t index)
	{
		// @todo: make this atomic
		std::lock_guard<std::mutex> lock(m_access_mutex);

		m_free_object_indices[m_free_object_count] = index;
		m_free_object_count++;

		return result::success;
	}

	/** @todo */
	result free(data_type* object)
	{
		// @todo: make this atomic
		std::lock_guard<std::mutex> lock(m_access_mutex);

		size_t index = (reinterpret_cast<char*>(object) - reinterpret_cast<char*>(m_objects)) / sizeof(data_type);

		m_free_object_indices[m_free_object_count] = index;
		m_free_object_count++;

		return result::success;
	}

	/** @todo */
	data_type* get_index(size_t index)
	{
		return &m_objects[index];
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
	size_t* m_free_object_indices = nullptr;

	/** @todo */
	size_t m_free_object_count = 0;

};

}; /* namespace internal */
}; /* namespace jobs */

#endif /* __JOBS_MEMORY_H__ */