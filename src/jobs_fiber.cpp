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

fiber::fiber(const memory_functions& memory_functions)
	: m_memory_functions(memory_functions)
{
}

fiber::~fiber()
{
}

result fiber::init(size_t stack_size, const fiber_entry_point& entry_point)
{
	m_entry_point = entry_point;

#ifdef JOBS_PLATFORM_WINDOWS
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
#else
#	error Unimplemented platform
#endif

	return result::success;
}

#ifdef JOBS_PLATFORM_WINDOWS
VOID CALLBACK fiber::trampoline_entry_point(PVOID lpParameter)
{
	fiber* this_fiber = reinterpret_cast<fiber*>(GetFiberData());
	this_fiber->m_entry_point();
}
#endif

}; /* namespace Jobs */