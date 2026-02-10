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

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <errno.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#endif

#include "HandleHandlingPool.hpp"

#ifdef _WIN32

HandleHandlingPool::HandleHandlingPool(size_t threads_per_pool, size_t max_handles_per_pool):
	threads_per_pool(threads_per_pool),
	max_handles_per_pool(max_handles_per_pool + 1),
	stopping(false)
{
	if(threads_per_pool < 1)
	{
		throw std::invalid_argument("threads_per_pool must be >= 1");
	}
	
	if(max_handles_per_pool < 1 || max_handles_per_pool >= MAXIMUM_WAIT_OBJECTS)
	{
		throw std::invalid_argument("max_handles_per_pool must be >= 1 and < MAXIMUM_WAIT_OBJECTS");
	}
	
	spin_workers = CreateEvent(NULL, TRUE, FALSE, NULL);
	if(spin_workers == NULL)
	{
		throw std::runtime_error("Unable to create event object");
	}
}

HandleHandlingPool::~HandleHandlingPool()
{
	/* Signal all active workers to stop and wait for any to exit. */
	
	stopping = true;
	SetEvent(spin_workers);
	
	std::unique_lock<std::mutex> wo_l(workers_lock);
	workers_cv.wait(wo_l, [this]() { return active_workers.empty(); });
	
	/* Reap the last thread which exited, if any were spawned. */
	
	if(join_worker.joinable())
	{
		join_worker.join();
	}
	
	CloseHandle(spin_workers);
}

void HandleHandlingPool::add_handle(HANDLE handle, const std::function<void()> &callback)
{
	/* See HandleHandlingPool.hpp for an explanation of this sequence. */
	
	std::unique_lock<std::mutex> pwl(pending_writer_lock);
	
	pending_writer = true;
	SetEvent(spin_workers);
	
	std::unique_lock<std::shared_mutex> wal(wait_lock);
	
	ResetEvent(spin_workers);
	pending_writer = false;
	
	pwl.unlock();
	
	/* Notify early and move any waiting workers up against wait_lock in case one of the below
	 * operations fails, else they may be left deadlocked.
	*/
	pending_writer_cv.notify_all();
	
	if((handles.size() % max_handles_per_pool) == 0)
	{
		/* There aren't any free slots for this handle in the currently running worker
		 * threads, start a new block beginning with spin_workers.
		*/
		
		size_t base_index = handles.size();
		
		handles.push_back(spin_workers);
		try {
			callbacks.push_back([](){ abort(); }); /* Callback should never be executed. */
			
			handles.push_back(handle);
			try {
				callbacks.push_back(callback);
			}
			catch(const std::exception &e)
			{
				handles.pop_back();
				throw e;
			}
		}
		catch(const std::exception &e)
		{
			handles.pop_back();
			throw e;
		}
		
		/* Calculate how many threads we need to spawn for there to be threads_per_pool
		 * workers for this pool.
		 *
		 * This may be less than threads_per_pool, if the block we just created previously
		 * existed, then some handles were removed causing it to go away - threads will exit
		 * once they detect they have nothing to wait for, but we may also add more handles
		 * before they catch up.
		 *
		 * Workers check if they have anything to do and remove themselves from
		 * active_workers while holding wait_lock, so there is no race between us counting
		 * the workers and them going away.
		*/
		
		std::unique_lock<std::mutex> wo_l(workers_lock);
		
		size_t threads_to_spawn = threads_per_pool;
		
		for(auto w = active_workers.begin(); w != active_workers.end(); w++)
		{
			if((*w)->base_index == base_index)
			{
				--threads_to_spawn;
			}
		}
		
		/* Spawn the new worker threads.
		 *
		 * We need to create the worker data on the heap so that:
		 *
		 * a) We can pass a reference to it into the worker main so that it may remove
		 *    itself from active_workers when the time comes.
		 *
		 * b) The reference in active_workers itself is const, so that thread may be
		 *    changed by the thread starting/exiting.
		 *
		 * The thread won't attempt to do anything (e.g. exit) until after acquiring
		 * wait_lock, so it won't attempt to do anything with its thread handle before it
		 * is done being initialised.
		*/
		
		for(size_t i = 0; i < threads_to_spawn; ++i)
		{
			Worker *w = new Worker(base_index);
			
			try {
				active_workers.insert(w);
				
				try {
					w->thread = std::thread(&HandleHandlingPool::worker_main, this, w);
				}
				catch(const std::exception &e)
				{
					active_workers.erase(w);
					throw e;
				}
			}
			catch(const std::exception &e)
			{
				delete w;
				
				if(i == 0)
				{
					/* This is the first worker we tried spawning, fail the
					 * whole operation.
					*/
					handles.pop_back(); callbacks.pop_back();
					handles.pop_back(); callbacks.pop_back();
					
					throw e;
				}
			}
		}
	}
	else{
		handles.push_back(handle);
		
		try {
			callbacks.push_back(callback);
		}
		catch(const std::exception &e)
		{
			handles.pop_back();
			throw e;
		}
	}
}

void HandleHandlingPool::remove_handle(HANDLE handle)
{
	/* See HandleHandlingPool.hpp for an explanation of this sequence. */
	
	std::unique_lock<std::mutex> pwl(pending_writer_lock);
	
	pending_writer = true;
	SetEvent(spin_workers);
	
	std::unique_lock<std::shared_mutex> wal(wait_lock);
	
	ResetEvent(spin_workers);
	pending_writer = false;
	
	pwl.unlock();
	
	/* Scan handles to find the index of the handle to be removed. */
	
	size_t remove_index = 0;
	while(remove_index < handles.size() && handles[remove_index] != handle)
	{
		++remove_index;
	}
	
	if(remove_index >= handles.size())
	{
		/* Couldn't find the handle. */
		return;
	}
	
	/* If the last handle is spin_workers, then the last call to remove_handle() removed the
	 * last handle in the last block. Now we remove it and the worker threads for that block
	 * will exit when they wake up.
	 *
	 * Doing it this way around means we destroy worker threads after removing its last handle
	 * and then another handle, reducing thrash if a single handle is added/removed at the
	 * boundary. Downside is we may keep one group of idle threads around for no reason.
	*/
	
	if(handles.back() == spin_workers)
	{
		handles.pop_back();
		callbacks.pop_back();
	}
	
	/* Replace it with the last handle. */
	
	handles[remove_index] = handles.back();
	handles.pop_back();
	
	callbacks[remove_index] = callbacks.back();
	callbacks.pop_back();
	
	pending_writer_cv.notify_all();
}

void HandleHandlingPool::worker_main(HandleHandlingPool::Worker *w)
{
	while(1)
	{
		if(pending_writer)
		{
			std::unique_lock<std::mutex> pwl(pending_writer_lock);
			pending_writer_cv.wait(pwl, [this]() { return !pending_writer; });
		}
		
		std::shared_lock<std::shared_mutex> l(wait_lock);
		
		if(handles.size() <= w->base_index)
		{
			/* No handles to wait on. Exit. */
			worker_exit(w);
			return;
		}
		
		/* Number of handles to wait for. We wait from base_index to the end of the handles
		 * array or the end of our block, whichever is closest.
		*/
		size_t num_handles = std::min((handles.size() - w->base_index), max_handles_per_pool);
		
		DWORD wait_res = WaitForMultipleObjects(num_handles, &(handles[w->base_index]), FALSE, INFINITE);
		
		if(stopping)
		{
			worker_exit(w);
			return;
		}
		
		if(wait_res >= (WAIT_OBJECT_0 + 1) && wait_res < (WAIT_OBJECT_0 + num_handles))
		{
			/* Take a copy of the callback functor so we can release all of our locks
			 * without worrying about it disappearing from under itself if handles are
			 * added/removed while its executing.
			*/
			size_t wait_index = (w->base_index + wait_res) - WAIT_OBJECT_0;
			std::function<void()> callback = callbacks[wait_index];
			
			l.unlock();
			
			callback();
		}
		else if(wait_res == WAIT_FAILED)
		{
			/* Some system error while waiting... invalid handle?
			 * Only thing we can do is go quietly. Or maybe not so quietly... abort?
			*/
			worker_exit(w);
			return;
		}
	}
}

/* Exit a worker thread.
 *
 * This MUST only be called by the worker thread which is about to exit, which
 * MUST exit as soon as this method returns.
*/
void HandleHandlingPool::worker_exit(HandleHandlingPool::Worker *w)
{
	std::unique_lock<std::mutex> wo_l(workers_lock);
	
	if(join_worker.joinable())
	{
		/* Only one zombie thread is allowed at a time. If one exists, we must reap it
		 * before taking its place.
		*/
		join_worker.join();
	}
	
	/* MSVC doesn't like assignment here for some reason. */
	join_worker.swap(w->thread);
	
	active_workers.erase(w);
	delete w;
	
	wo_l.unlock();
	workers_cv.notify_one();
	
	/* This thread is now ready to be reaped by the next thread to exit, or the destructor, if
	 * we are the last one.
	*/
}

HandleHandlingPool::Worker::Worker(size_t base_index):
	base_index(base_index) {}

#else /* Linux implementation */

HandleHandlingPool::Pool::Pool():
	handle_count(0)
{
	epoll_fd = epoll_create1(0);
	if(epoll_fd == -1)
	{
		throw std::runtime_error("Unable to create epoll fd");
	}

	wake_fd = eventfd(0, EFD_NONBLOCK);
	if(wake_fd == -1)
	{
		close(epoll_fd);
		throw std::runtime_error("Unable to create wake eventfd");
	}

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = wake_fd;

	if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, wake_fd, &ev) == -1)
	{
		close(wake_fd);
		close(epoll_fd);
		throw std::runtime_error("Unable to add wake_fd to epoll");
	}
}

HandleHandlingPool::Pool::~Pool()
{
	close(wake_fd);
	close(epoll_fd);
}

HandleHandlingPool::HandleHandlingPool(size_t threads_per_pool, size_t max_handles_per_pool):
	threads_per_pool(threads_per_pool),
	max_handles_per_pool(max_handles_per_pool > 0 ? max_handles_per_pool : 64),
	stopping(false)
{
	if(threads_per_pool < 1)
	{
		throw std::invalid_argument("threads_per_pool must be >= 1");
	}
}

HandleHandlingPool::~HandleHandlingPool()
{
	/* Signal all active workers to stop */
	stopping = true;

	/* Wake all pools */
	for(Pool *pool : pools)
	{
		uint64_t val = 1;
		write(pool->wake_fd, &val, sizeof(val));
	}

	std::unique_lock<std::mutex> wo_l(workers_lock);
	workers_cv.wait(wo_l, [this]() { return active_workers.empty(); });

	/* Reap the last thread which exited, if any were spawned. */
	if(join_worker.joinable())
	{
		join_worker.join();
	}

	/* Clean up pools */
	for(Pool *pool : pools)
	{
		delete pool;
	}
}

HandleHandlingPool::Pool* HandleHandlingPool::find_or_create_pool()
{
	/* Find an existing pool with space */
	for(Pool *pool : pools)
	{
		if(pool->handle_count < max_handles_per_pool)
		{
			return pool;
		}
	}

	/* Create a new pool */
	Pool *pool = new Pool();
	pools.push_back(pool);

	/* Spawn worker threads for this pool */
	spawn_pool_workers(pool);

	return pool;
}

void HandleHandlingPool::spawn_pool_workers(Pool *pool)
{
	std::unique_lock<std::mutex> wo_l(workers_lock);

	for(size_t i = 0; i < threads_per_pool; ++i)
	{
		Worker *w = new Worker(pool);

		try {
			active_workers.insert(w);
			pool->workers.insert(w);

			try {
				w->thread = std::thread(&HandleHandlingPool::worker_main, this, w);
			}
			catch(const std::exception &e)
			{
				pool->workers.erase(w);
				active_workers.erase(w);
				throw e;
			}
		}
		catch(const std::exception &e)
		{
			delete w;

			if(i == 0 && pools.size() == 1)
			{
				/* First worker of first pool failed - propagate error */
				throw e;
			}
		}
	}
}

void HandleHandlingPool::add_handle(int fd, const std::function<void()> &callback)
{
	std::unique_lock<std::shared_mutex> wal(wait_lock);

	Pool *pool = find_or_create_pool();

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = fd;

	if(epoll_ctl(pool->epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1)
	{
		throw std::runtime_error("Unable to add fd to epoll");
	}

	try {
		pool->callbacks[fd] = callback;
		fd_to_pool[fd] = pool;
		pool->handle_count++;
	}
	catch(const std::exception &e)
	{
		epoll_ctl(pool->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
		throw e;
	}

	/* Wake up worker threads in this pool to handle the new fd */
	uint64_t val = 1;
	write(pool->wake_fd, &val, sizeof(val));
}

void HandleHandlingPool::remove_handle(int fd)
{
	std::unique_lock<std::shared_mutex> wal(wait_lock);

	auto pool_it = fd_to_pool.find(fd);
	if(pool_it == fd_to_pool.end())
	{
		return;
	}

	Pool *pool = pool_it->second;

	epoll_ctl(pool->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
	pool->callbacks.erase(fd);
	fd_to_pool.erase(fd);
	pool->handle_count--;
}

void HandleHandlingPool::add_socket_monitor(int socket_fd, int eventfd, uint32_t events)
{
	std::unique_lock<std::shared_mutex> wal(wait_lock);

	Pool *pool = find_or_create_pool();

	struct epoll_event ev;
	ev.events = events;  /* EPOLLIN, EPOLLOUT, etc. */
	ev.data.fd = socket_fd;

	if(epoll_ctl(pool->epoll_fd, EPOLL_CTL_ADD, socket_fd, &ev) == -1)
	{
		throw std::runtime_error("Unable to add socket fd to epoll");
	}

	pool->socket_to_eventfd[socket_fd] = eventfd;
	fd_to_pool[socket_fd] = pool;
	pool->handle_count++;
}

void HandleHandlingPool::remove_socket_monitor(int socket_fd)
{
	std::unique_lock<std::shared_mutex> wal(wait_lock);

	auto pool_it = fd_to_pool.find(socket_fd);
	if(pool_it == fd_to_pool.end())
	{
		return;
	}

	Pool *pool = pool_it->second;

	epoll_ctl(pool->epoll_fd, EPOLL_CTL_DEL, socket_fd, NULL);
	pool->socket_to_eventfd.erase(socket_fd);
	fd_to_pool.erase(socket_fd);
	pool->handle_count--;
}

void HandleHandlingPool::worker_main(HandleHandlingPool::Worker *w)
{
	struct epoll_event events[64];
	Pool *pool = w->pool;

	while(!stopping)
	{
		/* Don't hold lock during epoll_wait - epoll_ctl is thread-safe
		 * and we only need to lock when accessing callbacks map */
		int nfds = epoll_wait(pool->epoll_fd, events, 64, -1);

		if(stopping)
		{
			worker_exit(w);
			return;
		}

		if(nfds == -1)
		{
			if(errno == EINTR)
			{
				continue;
			}

			/* Error in epoll_wait */
			worker_exit(w);
			return;
		}

		for(int i = 0; i < nfds; ++i)
		{
			int fd = events[i].data.fd;

			if(fd == pool->wake_fd)
			{
				/* Wake event - drain it and continue */
				uint64_t val;
				read(pool->wake_fd, &val, sizeof(val));
				continue;
			}

			/* Check if this is a socket fd being monitored */
			std::shared_lock<std::shared_mutex> l(wait_lock);
			auto socket_it = pool->socket_to_eventfd.find(fd);
			if(socket_it != pool->socket_to_eventfd.end())
			{
				/* This is a socket - signal its associated eventfd */
				int target_eventfd = socket_it->second;
				l.unlock();

				uint64_t val = 1;
				write(target_eventfd, &val, sizeof(val));
				continue;
			}

			/* This is an eventfd - drain it and invoke callback.
			 * With level-triggered epoll, multiple threads may wake for the same event.
			 * Only the thread that successfully reads the eventfd should invoke the callback.
			 */
			uint64_t val;
			if(read(fd, &val, sizeof(val)) == sizeof(val))
			{
				auto it = pool->callbacks.find(fd);
				if(it != pool->callbacks.end())
				{
					/* Copy callback so we can release lock before invoking */
					std::function<void()> callback = it->second;
					l.unlock();

					callback();
				}
				else
				{
					l.unlock();
				}
			}
		}
	}

	worker_exit(w);
}

void HandleHandlingPool::worker_exit(HandleHandlingPool::Worker *w)
{
	std::unique_lock<std::mutex> wo_l(workers_lock);

	if(join_worker.joinable())
	{
		join_worker.join();
	}

	join_worker.swap(w->thread);

	w->pool->workers.erase(w);
	active_workers.erase(w);
	delete w;

	wo_l.unlock();
	workers_cv.notify_one();
}

HandleHandlingPool::Worker::Worker(Pool *pool):
	pool(pool) {}

#endif
