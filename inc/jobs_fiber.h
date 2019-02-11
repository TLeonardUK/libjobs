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
 *  \file jobs_fiber.h
 *
 *  Include header for fiber management functionality.
 */

#ifndef __JOBS_FIBER_H__
#define __JOBS_FIBER_H__

#include "jobs_defines.h"
#include "jobs_enums.h"
#include "jobs_memory.h"

#include <functional>

#if defined(JOBS_PLATFORM_PS4)
#include <sce_fiber.h>
#elif defined(JOBS_PLATFORM_SWITCH)
#include <nn/os.h>
#endif

namespace jobs {
namespace internal {
    
/**
 *  \brief Entry point for a fiber.
 */
typedef std::function<void()> fiber_entry_point;

/**
 *  Encapsulates a single user-space thread's (aka. coroutine/fiber) context of execution.
 */
class fiber
{
public:

    /**
     * Default constructor.
     */
    fiber();

    /**
     * Constructor.
     *
     * \param memory_function Scheduler defined memory functions used to 
     *						  override default memory allocation behaviour.
     */
    fiber(const memory_functions& memory_functions);

    /** Destructor. */
    ~fiber();

    /** Disposes of all resources. Performed automatically in destructor if not called beforehand. */
    void destroy();

    /**
     * \brief Initializes this fiber.
     *
     * \param stack_size Size of the stack that should be allocated for this fibers execution context.
     * \param entry_point Function that this fiber should start running when executed.
     * \param name Descriptive name of this fiber.
     *
     * \return Value indicating the success of this function.
     */
    result init(size_t stack_size, const fiber_entry_point& entry_point, const char* name);

    /**
     * \brief Switches this threads execution context to this fiber.
     *
     * \return Value indicating the success of this function.
     */
    result switch_to();

    /**
     * \brief Converts the current thread to a fiber.
     * 
     *  On some operating systems (notably windows) fibers can only be run by other fibers.
     *  so any worker threads dealing with fibers need to first be converted. Which is done using
     *  this function.
     */
    static void convert_thread_to_fiber(fiber& result);


    /**
     * \brief Converts the current fiber to a thread.
     *
     *  This performs the inverse of convert_thread_to_fiber and should be invoked when the thread
     *  has finished performing any fiber operations.
     */
    static void convert_fiber_to_thread();

private:

#if defined(JOBS_PLATFORM_WINDOWS) || defined(JOBS_PLATFORM_XBOX_ONE)
    
    /** Trampoline function used to call the user-defined entry point. */
    static VOID CALLBACK trampoline_entry_point(PVOID lpParameter);

#elif defined(JOBS_PLATFORM_PS4)

    /** Trampoline function used to call the user-defined entry point. */
    __attribute__((noreturn))
    static void trampoline_entry_point(uint64_t argOnInitialize, uint64_t argOnRun);

#elif defined(JOBS_PLATFORM_SWITCH)

    /** Trampoline function used to call the user-defined entry point. */
    static nn::os::FiberType* trampoline_entry_point(void* arg);

#endif

private:

    /** Memory allocation functions provided by the scheduler. */
    memory_functions m_memory_functions;

    /** User defined entry point to execute when the fiber is run. */
    fiber_entry_point m_entry_point;

    /** Stack space allocated to fiber context. */
    bool m_is_thread = false;

    /** Stack space allocated to fiber context. */
    void* m_fiber_context = nullptr;

#if defined(JOBS_PLATFORM_WINDOWS) || defined(JOBS_PLATFORM_XBOX_ONE)

    /** Handle of platform defined fiber. */
    LPVOID m_fiber_handle = nullptr;

#elif defined(JOBS_PLATFORM_PS4)

    /** Handle of platform defined fiber. */
    SceFiber m_fiber_handle __attribute__((aligned(SCE_FIBER_ALIGNMENT)));

    /** True if fiber handle is valid. */
    bool m_fiber_handle_created = false;

#elif defined(JOBS_PLATFORM_SWITCH)

    /** Handle of platform defined fiber. */
    nn::os::FiberType m_fiber_handle;

    /** True if fiber handle is valid. */
    bool m_fiber_handle_created = false;

#endif

};

}; /* namespace internal */
}; /* namespace jobs */

#endif /* __JOBS_FIBER_H__ */