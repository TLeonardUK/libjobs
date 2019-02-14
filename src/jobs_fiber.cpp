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

#include "jobs_fiber.h"

#include <cassert>

namespace jobs {
namespace internal {

fiber::fiber()
{
}

fiber::fiber(const memory_functions& memory_functions)
    : m_memory_functions(memory_functions)
{
}

fiber::~fiber()
{
    destroy();
}

void fiber::destroy()
{
#if defined(JOBS_PLATFORM_WINDOWS) || defined(JOBS_PLATFORM_XBOX_ONE)

    if (m_fiber_handle != nullptr && !m_is_thread)
    {
        DeleteFiber(m_fiber_handle);
        m_fiber_handle = nullptr;
    }

#elif defined(JOBS_PLATFORM_PS4)

    if (m_fiber_handle_created && !m_is_thread)
    {
        sceFiberFinalize(&m_fiber_handle);
        m_fiber_handle_created = false;
    }

#elif defined(JOBS_PLATFORM_SWITCH)

    if (m_fiber_handle_created && !m_is_thread)
    {
        nn::os::FinalizeFiber(&m_fiber_handle);
        m_fiber_handle_created = false;
    }

#else

#	error Unimplemented platform

#endif

    if (m_fiber_context != nullptr)
    {
        m_memory_functions.user_free(m_fiber_context);
        m_fiber_context = nullptr;
    }
}

result fiber::init(size_t stack_size, const fiber_entry_point& entry_point, const char* name)
{
    m_entry_point = entry_point;

#if defined(JOBS_PLATFORM_WINDOWS) || defined(JOBS_PLATFORM_XBOX_ONE)

    m_fiber_handle = CreateFiberEx(stack_size, stack_size, FIBER_FLAG_FLOAT_SWITCH, trampoline_entry_point, this);
    if (m_fiber_handle == nullptr)
    {
        // Try and figure out some useful results if possible.
        DWORD error = GetLastError();
        if (error == ERROR_OUTOFMEMORY)
        {
            return result::out_of_memory;
        }

        return result::platform_error;
    }

#elif defined(JOBS_PLATFORM_PS4)

    m_fiber_context = m_memory_functions.user_alloc(stack_size, SCE_FIBER_CONTEXT_ALIGNMENT);
    if (m_fiber_context == nullptr)
    {
        return result::out_of_memory;
    }

    int32_t ret = sceFiberInitialize(&m_fiber_handle, name, trampoline_entry_point, reinterpret_cast<uint64_t>(this), m_fiber_context, stack_size, nullptr);
    if (ret != SCE_OK)
    {
        return result::platform_error;
    }

    m_fiber_handle_created = true;

#elif defined(JOBS_PLATFORM_SWITCH)

    // Size of stack must be aligned to this (there are no notes about this in the api
    // docs, but it will assert if you don't ...). Note this value is not the same as nn::os::FiberStackAlignment.
    static const int stack_size_alignment = 0x1000; 
    stack_size = ((stack_size + stack_size_alignment - 1) / stack_size_alignment) * stack_size_alignment;

    m_fiber_context = m_memory_functions.user_alloc(stack_size, nn::os::FiberStackAlignment);
    if (m_fiber_context == nullptr)
    {
        return result::out_of_memory;
    }

    nn::os::InitializeFiber(&m_fiber_handle, trampoline_entry_point, reinterpret_cast<void*>(this), m_fiber_context, stack_size, 0);

    m_fiber_handle_created = true;

#else

#	error Unimplemented platform

#endif

    return result::success;
}

void fiber::convert_thread_to_fiber(fiber& result)
{
    result.m_entry_point = nullptr;
    result.m_is_thread = true;

#if defined(JOBS_PLATFORM_WINDOWS) || defined(JOBS_PLATFORM_XBOX_ONE)

    result.m_fiber_handle = ConvertThreadToFiberEx(nullptr, FIBER_FLAG_FLOAT_SWITCH);

#elif defined(JOBS_PLATFORM_PS4)

    // Nothing to do.

#elif defined(JOBS_PLATFORM_SWITCH)

    // Nothing to do.

#else

#	error Unimplemented platform

#endif
}

void fiber::convert_fiber_to_thread()
{
#if defined(JOBS_PLATFORM_WINDOWS) || defined(JOBS_PLATFORM_XBOX_ONE)

    ConvertFiberToThread();

#elif defined(JOBS_PLATFORM_PS4)

    //Nothing to do.

#elif defined(JOBS_PLATFORM_SWITCH)

    //Nothing to do.

#else

#	error Unimplemented platform

#endif
}

result fiber::switch_to()
{
#if defined(JOBS_PLATFORM_WINDOWS) || defined(JOBS_PLATFORM_XBOX_ONE)

    SwitchToFiber(m_fiber_handle);

#elif defined(JOBS_PLATFORM_PS4)

    if (m_is_thread)
    {
        int32_t ret = sceFiberReturnToThread(0, 0);
        if (ret != SCE_OK)
        {
            return result::platform_error;
        }
    }
    else
    {
        int32_t ret = sceFiberRun(&m_fiber_handle, 0, 0);
        if (ret != SCE_OK)
        {
            return result::platform_error;
        }
    }

#elif defined(JOBS_PLATFORM_SWITCH)

    if (m_is_thread)
    {
        nn::os::SwitchToFiber(nullptr);
    }
    else
    {
        nn::os::SwitchToFiber(&m_fiber_handle);
    }

#else

#	error Unimplemented platform

#endif

    return result::success;
}

#if defined(JOBS_PLATFORM_WINDOWS) || defined(JOBS_PLATFORM_XBOX_ONE)

VOID CALLBACK fiber::trampoline_entry_point(PVOID lpParameter)
{
    fiber* this_fiber = reinterpret_cast<fiber*>(GetFiberData());
    this_fiber->m_entry_point();
}

#elif defined(JOBS_PLATFORM_PS4)

void fiber::trampoline_entry_point(uint64_t argOnInitialize, uint64_t argOnRun)
{
    fiber* this_fiber = reinterpret_cast<fiber*>(argOnInitialize);
    this_fiber->m_entry_point();    
}

#elif defined(JOBS_PLATFORM_SWITCH)

nn::os::FiberType* fiber::trampoline_entry_point(void* arg)
{
    fiber* this_fiber = reinterpret_cast<fiber*>(arg);
    this_fiber->m_entry_point();
    return nullptr;
}

#endif

}; /* namespace internal */
}; /* namespace jobs */