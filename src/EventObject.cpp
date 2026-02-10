/* DirectPlay Lite
 * Copyright (C) 2018 Daniel Collins <solemnwarning@solemnwarning.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <stdexcept>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/eventfd.h>
#include <unistd.h>
#endif

#include "EventObject.hpp"

EventObject::EventObject(BOOL bManualReset, BOOL bInitialState)
#ifdef _WIN32
{
	handle = CreateEvent(NULL, bManualReset, bInitialState, NULL);
	if(handle == NULL)
	{
		throw std::runtime_error("Unable to create event object");
	}
}
#else
	: manual_reset(bManualReset), signaled(false)
{
	/* For manual-reset events, use a large initial value so multiple threads can each
	 * read once from the semaphore-mode eventfd. For auto-reset, only one thread should
	 * wake per signal.
	 */
	unsigned int initial_val = 0;
	if(bInitialState)
	{
		initial_val = bManualReset ? 1024 : 1;
	}
	fd = eventfd(initial_val, EFD_NONBLOCK | (bManualReset ? EFD_SEMAPHORE : 0));
	if(fd == -1)
	{
		throw std::runtime_error("Unable to create event object");
	}
	signaled = bInitialState;
}
#endif

EventObject::~EventObject()
{
#ifdef _WIN32
	CloseHandle(handle);
#else
	close(fd);
#endif
}

#ifdef _WIN32
EventObject::operator HANDLE() const
{
	return handle;
}
#else
EventObject::operator int() const
{
	return fd;
}

void EventObject::set()
{
	/* For manual-reset events, write a large value so multiple threads can each read once.
	 * For auto-reset events, write 1 so only one thread wakes per signal.
	 * With EFD_SEMAPHORE, writes add to the counter and reads decrement by 1.
	 */
	uint64_t val = manual_reset ? 1024 : 1;
	ssize_t result = write(fd, &val, sizeof(val));
	(void)result;
	signaled = true;
}

void EventObject::reset()
{
	uint64_t val;
	if(manual_reset)
	{
		while(read(fd, &val, sizeof(val)) == sizeof(val))
		{
			/* Keep draining until EAGAIN */
		}
	}
	else
	{
		read(fd, &val, sizeof(val));
	}
	signaled = false;
}
#endif
