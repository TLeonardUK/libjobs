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

#include "jobs_thread.h"

#include <cassert>
#include <cstdlib>

#if defined(JOBS_PLATFORM_WINDOWS)
#include <windows.h>
#include <processthreadsapi.h>
#endif

namespace jobs {
namespace internal {

thread::thread(const memory_functions& memory_functions)
    : m_memory_functions(memory_functions)
{
}

thread::~thread()
{
    join();
}

result thread::init(const thread_entry_point& entry_point, const char* name)
{
#if defined(JOBS_PLATFORM_WINDOWS)

    wchar_t wide_name[128];
    std::mbstate_t state = std::mbstate_t();
    size_t ret_size = 0;
    mbsrtowcs_s(&ret_size, wide_name, &name, 128, &state);

    std::thread new_thread([entry_point, wide_name]() {
        SetThreadDescription(GetCurrentThread(), wide_name);
        entry_point();
    });

#elif defined(JOBS_PLATFORM_PS4)
    
    std::thread new_thread([entry_point, name]() {
        scePthreadRename(scePthreadSelf(), name);
        entry_point();
    });

#else

    std::thread new_thread([entry_point]() {
        entry_point();
    });

#endif
    m_thread.swap(new_thread);

    assert(m_thread.joinable());

    return result::success;
}

void thread::join()
{
    if (m_thread.joinable())
    {
        m_thread.join();
    }
}

}; /* namespace internal */
}; /* namespace jobs */