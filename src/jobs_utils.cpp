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

#include "jobs_utils.h"

namespace jobs {
namespace internal {

void stopwatch::start()
{
    m_start_time = std::chrono::high_resolution_clock::now();
    m_has_end = false;
}

void stopwatch::stop()
{
    m_end_time = std::chrono::high_resolution_clock::now();
    m_has_end = true;
}

uint64_t stopwatch::get_elapsed_ms()
{
    std::chrono::high_resolution_clock::time_point end_time;
    if (m_has_end)
    {
        end_time = m_end_time;
    }
    else
    {
        end_time = std::chrono::high_resolution_clock::now();
    }

    float elapsed = std::chrono::duration<float, std::chrono::milliseconds::period>(end_time - m_start_time).count();

    return (uint64_t)elapsed;
}

uint64_t stopwatch::get_elapsed_us()
{
    std::chrono::high_resolution_clock::time_point end_time;
    if (m_has_end)
    {
        end_time = m_end_time;
    }
    else
    {
        end_time = std::chrono::high_resolution_clock::now();
    }

    float elapsed = std::chrono::duration<float, std::chrono::microseconds::period>(end_time - m_start_time).count();

    return (uint64_t)elapsed;
}

}; /* namespace internal */

const timeout timeout::infinite = timeout(UINT64_MAX);

}; /* namespace jobs */