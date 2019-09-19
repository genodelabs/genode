/*
 * \brief  poll() implementation
 * \author Josef Soentgen
 * \author Emery Hemingway
 * \date   2012-07-12
 */

/*
 * Copyright (C) 2010-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Libc includes */
#include <libc-plugin/plugin_registry.h>
#include <libc-plugin/plugin.h>
#include <sys/poll.h>

/* internal includes */
#include <internal/errno.h>
#include <internal/file.h>
#include <internal/init.h>
#include <internal/suspend.h>

using namespace Libc;


static Suspend *_suspend_ptr;


void Libc::init_poll(Suspend &suspend)
{
	_suspend_ptr = &suspend;
}


extern "C" __attribute__((weak))
int poll(struct pollfd fds[], nfds_t nfds, int timeout_ms)
{
	using Genode::uint64_t;

	if (!fds || nfds == 0) return Errno(EINVAL);

	struct Check : Suspend_functor
	{
		pollfd       *_fds;
		nfds_t const  _nfds;

		int nready { 0 };

		Check(struct pollfd fds[], nfds_t nfds)
		: _fds(fds), _nfds(nfds) { }

		bool suspend() override
		{
			bool polling = false;

			for (unsigned i = 0; i < _nfds; ++i)
			{
				pollfd &pfd = _fds[i];
				File_descriptor *libc_fd = libc_fd_to_fd(pfd.fd, "poll");
				if (!libc_fd) {
					pfd.revents |= POLLNVAL;
					++nready;
					continue;
				}

				if (!libc_fd->plugin || !libc_fd->plugin->supports_poll()) {
					warning("poll not supported for file descriptor ", pfd.fd);
					continue;
				}

				nready += libc_fd->plugin->poll(*libc_fd, pfd);
				polling = true;
			}

			return (polling && nready == 0);
		}

	} check (fds, nfds);

	check.suspend();

	if (timeout_ms == 0) {
		return check.nready;
	}

	auto suspend = [&] (uint64_t timeout_ms)
	{
		struct Missing_call_of_init_poll : Exception { };
		if (!_suspend_ptr)
			throw Missing_call_of_init_poll();

		return _suspend_ptr->suspend(check, timeout_ms);
	};

	if (timeout_ms == -1) {
		while (check.nready == 0) {
			suspend(0);
		}
	} else {
		uint64_t remaining_ms = timeout_ms;
		while (check.nready == 0 && remaining_ms > 0) {
			remaining_ms = suspend(remaining_ms);
		}
	}

	return check.nready;
}


extern "C" __attribute__((weak, alias("poll")))
int __sys_poll(struct pollfd fds[], nfds_t nfds, int timeout_ms);


extern "C" __attribute__((weak, alias("poll")))
int _poll(struct pollfd fds[], nfds_t nfds, int timeout_ms);


extern "C" __attribute__((weak))
int ppoll(struct pollfd fds[], nfds_t nfds,
          const struct timespec *timeout,
          const sigset_t*)
{
	int timeout_ms = timeout->tv_sec * 1000 + timeout->tv_nsec / 1000;
	return poll(fds, nfds, timeout_ms);
}


extern "C" __attribute__((weak, alias("ppoll")))
int __sys_ppoll(struct pollfd fds[], nfds_t nfds,
                const struct timespec *timeout,
                const sigset_t*);
