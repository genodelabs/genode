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
#include "libc_file.h"
#include "task.h"


extern "C" __attribute__((weak))
int poll(struct pollfd fds[], nfds_t nfds, int timeout_ms)
{
	using namespace Libc;

	struct Check : Libc::Suspend_functor
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
					Genode::warning("poll not supported for file descriptor ", pfd.fd);
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

	if (timeout_ms == -1) {
		while (check.nready == 0) {
			Libc::suspend(check, 0);
		}
	} else {
		Genode::uint64_t remaining_ms = timeout_ms;
		while (check.nready == 0 && remaining_ms > 0) {
			remaining_ms = Libc::suspend(check, remaining_ms);
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
