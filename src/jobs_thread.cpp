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

namespace jobs {

thread::thread(const memory_functions& memory_functions)
	: m_memory_functions(memory_functions)
{
}

thread::~thread()
{
}

result thread::init(const thread_entry_point& entry_point)
{
	std::thread new_thread(entry_point);
	m_thread.swap(new_thread);

	assert(m_thread.joinable());

	return result::success;
}

void thread::join()
{
	m_thread.join();
}

}; /* namespace Jobs */