/*
 * \brief  select() implementation
 * \author Christian Prochaska
 * \author Christian Helmuth
 * \author Emery Hemingway
 * \author Norman Feske
 * \date   2010-01-21
 *
 * Note what POSIX states about select(): File descriptors associated with
 * regular files always select true for ready to read, ready to write, and
 * error conditions.
 */

/*
 * Copyright (C) 2010-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/exception.h>
#include <util/reconstructible.h>

/* Libc includes */
#include <libc/select.h>
#include <stdlib.h>
#include <sys/select.h>
#include <signal.h>

/* libc-internal includes */
#include <internal/plugin_registry.h>
#include <internal/plugin.h>
#include <internal/kernel.h>
#include <internal/init.h>
#include <internal/signal.h>
#include <internal/select.h>
#include <internal/errno.h>
#include <internal/monitor.h>

using namespace Libc;


static Select       *_select_ptr;


void Libc::init_select(Select &select)
{
	_select_ptr  = &select;
}


static int poll_nfds_from_select_nfds(int select_nfds,
                                      fd_set &readfds,
                                      fd_set &writefds,
                                      fd_set &exceptfds)
{
	int poll_nfds = 0;

	for (int fd = 0; fd < select_nfds; fd++) {

		bool fd_in_readfds = FD_ISSET(fd, &readfds);
		bool fd_in_writefds = FD_ISSET(fd, &writefds);
		bool fd_in_exceptfds = FD_ISSET(fd, &exceptfds);

		if (fd_in_readfds || fd_in_writefds || fd_in_exceptfds)
			poll_nfds++;
	}

	return poll_nfds;
}


void populate_pollfds(struct pollfd pollfds[], int poll_nfds,
                      int select_nfds,
                      fd_set &readfds, fd_set &writefds, fd_set &exceptfds)
{
	for (int fd = 0, pollfd_index = 0; fd < select_nfds; fd++) {

		bool fd_in_readfds = FD_ISSET(fd, &readfds);
		bool fd_in_writefds = FD_ISSET(fd, &writefds);
		bool fd_in_exceptfds = FD_ISSET(fd, &exceptfds);

		if (!fd_in_readfds && !fd_in_writefds && !fd_in_exceptfds)
			continue;

		pollfds[pollfd_index].fd = fd;

		pollfds[pollfd_index].events = 0;

		if (fd_in_readfds)
			pollfds[pollfd_index].events |= POLLIN;

		if (fd_in_writefds)
			pollfds[pollfd_index].events |= POLLOUT;

		if (fd_in_exceptfds)
			pollfds[pollfd_index].events |= POLLERR;

		pollfd_index++;
	}
}


extern "C" __attribute__((weak))
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
           struct timeval *tv)
{
	/*
	 * Create copies of the input sets or new empty input sets to avoid
	 * repeated nullpointer checks later on.
	 */

	fd_set in_readfds, in_writefds, in_exceptfds;

	if (readfds)   in_readfds   = *readfds;   else FD_ZERO(&in_readfds);
	if (writefds)  in_writefds  = *writefds;  else FD_ZERO(&in_writefds);
	if (exceptfds) in_exceptfds = *exceptfds; else FD_ZERO(&in_exceptfds);

	int poll_nfds = poll_nfds_from_select_nfds(nfds, in_readfds, in_writefds, in_exceptfds);

	struct pollfd pollfds[poll_nfds];

	populate_pollfds(pollfds, poll_nfds,
	                 nfds, in_readfds, in_writefds, in_exceptfds);

	int const timeout_ms = (tv != nullptr)
	                       ? tv->tv_sec*1000 + tv->tv_usec/1000
	                       : -1;

	int poll_nready = poll(pollfds, poll_nfds, timeout_ms);

	if (poll_nready < 0)
		return poll_nready;

	if (readfds)   FD_ZERO(readfds);
	if (writefds)  FD_ZERO(writefds);
	if (exceptfds) FD_ZERO(exceptfds);

	int nready = 0;

	if (poll_nready > 0) {

		for (int pollfd_index = 0;
		     pollfd_index < poll_nfds;
		     pollfd_index++) {

			if (readfds && (pollfds[pollfd_index].revents & POLLIN)) {
				FD_SET(pollfds[pollfd_index].fd, readfds);
				nready++;
			}

			if (writefds && (pollfds[pollfd_index].revents & POLLOUT)) {
				FD_SET(pollfds[pollfd_index].fd, writefds);
				nready++;
			}

			if (exceptfds && (pollfds[pollfd_index].revents & POLLERR)) {
				FD_SET(pollfds[pollfd_index].fd, exceptfds);
				nready++;
			}
		}
	}

	return nready;
}


extern "C" __attribute__((alias("select")))
int __sys_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
                 struct timeval *tv);

extern "C" __attribute__((alias("select")))
int _select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
            struct timeval *tv);


extern "C" __attribute__((weak))
int pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
            const struct timespec *timeout, const sigset_t *sigmask)
{
	struct timeval tv { 0, 0 };
	struct timeval *tv_ptr = nullptr;
	sigset_t origmask;
	int nready;

	if (timeout) {
		tv.tv_usec = timeout->tv_nsec / 1000;
		tv.tv_sec = timeout->tv_sec;
		tv_ptr = &tv;
	}

	if (sigmask)
		sigprocmask(SIG_SETMASK, sigmask, &origmask);

	nready = select(nfds, readfds, writefds, exceptfds, tv_ptr);

	if (sigmask)
		sigprocmask(SIG_SETMASK, &origmask, NULL);

	return nready;
}

extern "C" __attribute__((alias("pselect")))
int __sys_pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
                  const struct timespec *timeout, const sigset_t *sigmask);


/****************************************
 ** Select handler for libc components **
 ****************************************/

int Libc::Select_handler_base::select(int nfds, fd_set &readfds,
                                      fd_set &writefds, fd_set &exceptfds)
{
	/*
	 * Save the input data before calling '::select()' which zeroes out
	 * the fd sets if nothing is ready.
	 */
	_nfds = nfds;
	_readfds = readfds;
	_writefds = writefds;
	_exceptfds = exceptfds;

	struct timeval tv { 0, 0 };

	int const nready = ::select(nfds, &readfds, &writefds, &exceptfds, &tv);

	/* return if any descripor is ready or an error occurred */
	if (nready != 0)
		return nready;

	struct Missing_call_of_init_select : Exception { };
	if (!_select_ptr)
		throw Missing_call_of_init_select();

	_select_ptr->schedule_select(*this);

	return 0;
}


void Libc::Select_handler_base::dispatch_select()
{
	/* '::select()' zeroes out the fd sets if nothing is ready */
	fd_set tmp_readfds = _readfds;
	fd_set tmp_writefds = _writefds;
	fd_set tmp_exceptfds = _exceptfds;

	struct timeval tv { 0, 0 };

	int const nready = ::select(_nfds,
	                            &tmp_readfds,
	                            &tmp_writefds,
	                            &tmp_exceptfds,
	                            &tv);

	if (nready == 0) return;

	if (_select_ptr)
		_select_ptr->deschedule_select();

	select_ready(nready, tmp_readfds, tmp_writefds, tmp_exceptfds);
}
