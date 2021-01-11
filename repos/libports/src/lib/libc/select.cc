/*
 * \brief  select() implementation
 * \author Christian Prochaska
 * \author Christian Helmuth
 * \author Emery Hemingway
 * \author Norman Feske
 * \date   2010-01-21
 *
 * the 'select()' implementation is partially based on the lwip version as
 * implemented in 'src/api/sockets.c'
 *
 * Note what POSIX states about select(): File descriptors associated with
 * regular files always select true for ready to read, ready to write, and
 * error conditions.
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/exception.h>
#include <util/reconstructible.h>

/* Libc includes */
#include <libc-plugin/plugin_registry.h>
#include <libc-plugin/plugin.h>
#include <libc/select.h>
#include <stdlib.h>
#include <sys/select.h>
#include <signal.h>

/* libc-internal includes */
#include <internal/kernel.h>
#include <internal/init.h>
#include <internal/signal.h>
#include <internal/select.h>
#include <internal/errno.h>
#include <internal/monitor.h>

namespace Libc {
	struct Select_cb;
	struct Select_cb_list;
}

using namespace Libc;


static Select       *_select_ptr;
static Libc::Signal *_signal_ptr;
static Monitor      *_monitor_ptr;


void Libc::init_select(Select &select, Signal &signal, Monitor &monitor)
{
	_select_ptr  = &select;
	_signal_ptr  = &signal;
	_monitor_ptr = &monitor;
}


/** Description for a task waiting in select */
struct Libc::Select_cb
{
	Select_cb *next = nullptr;

	int const nfds;
	int       nready = 0;
	fd_set    readfds;
	fd_set    writefds;
	fd_set    exceptfds;

	Select_cb(int nfds, fd_set const &readfds, fd_set const &writefds, fd_set const &exceptfds)
	:
		nfds(nfds), readfds(readfds), writefds(writefds), exceptfds(exceptfds)
	{ }
};


struct Libc::Select_cb_list
{
	Mutex      _mutex;
	Select_cb *_first = nullptr;

	struct Guard : Mutex::Guard
	{
		Select_cb_list *l;

		Guard(Select_cb_list &list) : Mutex::Guard(list._mutex), l(&list) { }
	};

	void unsynchronized_insert(Select_cb *scb)
	{
		scb->next = _first;
		_first = scb;
	}

	void insert(Select_cb *scb)
	{
		Guard guard(*this);
		unsynchronized_insert(scb);
	}

	void remove(Select_cb *scb)
	{
		Guard guard(*this);

		/* address of pointer to next allows to change the head */
		for (Select_cb **next = &_first; *next; next = &(*next)->next) {
			if (*next == scb) {
				*next = scb->next;
				break;
			}
		}
	}

	template <typename FUNC>
	void for_each(FUNC const &func)
	{
		Guard guard(*this);

		for (Select_cb *scb = _first; scb; scb = scb->next)
			func(*scb);
	}
};

/** The global list of tasks waiting for select */
static Libc::Select_cb_list &select_cb_list()
{
	static Select_cb_list inst;
	return inst;
}


/**
 * Poll plugin select() functions
 *
 * We iterate over all file descriptors in each list and count the number of
 * ready descriptors. Output file-descriptor sets are cleared by this function
 * (according to POSIX).
 */
static int selscan(int nfds,
                   fd_set *in_readfds,  fd_set *in_writefds,  fd_set *in_exceptfds,
                   fd_set *out_readfds, fd_set *out_writefds, fd_set *out_exceptfds)
{
	int nready = 0;

	 /* zero timeout for polling of the plugins' select() functions */
	struct timeval tv_0 = { 0, 0 };

	/* temporary fd sets that are passed to the plugins */
	int    plugin_nready;
	fd_set plugin_readfds;
	fd_set plugin_writefds;
	fd_set plugin_exceptfds;

	/* clear fd sets */
	if (out_readfds)   FD_ZERO(out_readfds);
	if (out_writefds)  FD_ZERO(out_writefds);
	if (out_exceptfds) FD_ZERO(out_exceptfds);

	if (nfds > FD_SETSIZE)
		return Libc::Errno(EINVAL);

	for (Plugin *plugin = plugin_registry()->first();
	     plugin;
	     plugin = plugin->next()) {
		if (plugin->supports_select(nfds, in_readfds, in_writefds, in_exceptfds, &tv_0)) {

			plugin_readfds = *in_readfds;
			plugin_writefds = *in_writefds;
			plugin_exceptfds = *in_exceptfds;

			plugin_nready = plugin->select(nfds, &plugin_readfds, &plugin_writefds, &plugin_exceptfds, &tv_0);

			if (plugin_nready > 0) {
				for (int libc_fd = 0; libc_fd < nfds; libc_fd++) {
					if (out_readfds && FD_ISSET(libc_fd, &plugin_readfds)) {
						FD_SET(libc_fd, out_readfds);
					}
					if (out_writefds && FD_ISSET(libc_fd, &plugin_writefds)) {
						FD_SET(libc_fd, out_writefds);
					}
					if (out_exceptfds && FD_ISSET(libc_fd, &plugin_exceptfds)) {
						FD_SET(libc_fd, out_exceptfds);
					}
				}
				nready += plugin_nready;
			} else if (plugin_nready < 0) {
				error("plugin->select() returned error value ", plugin_nready);
			}
		}
	}

	return nready;
}


/* this function gets called by plugin backends when file descripors become ready */
void Libc::select_notify_from_kernel()
{
	fd_set tmp_readfds, tmp_writefds, tmp_exceptfds;

	/* check for each waiting select() function if one of its fds is ready now
	 * and if so, wake all up */

	select_cb_list().for_each([&] (Select_cb &scb) {
		scb.nready = selscan(scb.nfds,
		                     &scb.readfds, &scb.writefds, &scb.exceptfds,
		                     &tmp_readfds,  &tmp_writefds,  &tmp_exceptfds);
		if (scb.nready > 0) {
			scb.readfds   = tmp_readfds;
			scb.writefds  = tmp_writefds;
			scb.exceptfds = tmp_exceptfds;
		}
	});
}


extern "C" __attribute__((weak))
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
           struct timeval *tv)
{
	fd_set in_readfds, in_writefds, in_exceptfds;

	Constructible<Select_cb> select_cb;

	if (readfds)   in_readfds   = *readfds;   else FD_ZERO(&in_readfds);
	if (writefds)  in_writefds  = *writefds;  else FD_ZERO(&in_writefds);
	if (exceptfds) in_exceptfds = *exceptfds; else FD_ZERO(&in_exceptfds);

	/*
	 * Insert callback first to avoid race after 'selscan()'
	 */

	select_cb.construct(nfds, in_readfds, in_writefds, in_exceptfds);
	select_cb_list().insert(&(*select_cb));

	int const nready = selscan(nfds,
	                           &in_readfds, &in_writefds, &in_exceptfds,
	                           readfds, writefds, exceptfds);

	/* return if any descripor is ready */
	if (nready) {
		select_cb_list().remove(&(*select_cb));
		return nready;
	}

	/* return on zero-timeout */
	if (tv && (tv->tv_sec) == 0 && (tv->tv_usec == 0)) {
		select_cb_list().remove(&(*select_cb));
		return 0;
	}

	using Genode::uint64_t;

	uint64_t const timeout_ms = (tv != nullptr)
	                          ? (uint64_t)tv->tv_sec*1000 + tv->tv_usec/1000
	                          : 0UL;
	{
		struct Missing_call_of_init_select : Exception { };
		if (!_monitor_ptr || !_signal_ptr)
			throw Missing_call_of_init_select();
	}

	unsigned const orig_signal_count = _signal_ptr->count();

	auto signal_occurred_during_select = [&] ()
	{
		return (_signal_ptr->count() != orig_signal_count);
	};

	auto monitor_fn = [&] ()
	{
		select_notify_from_kernel();

		if (select_cb->nready != 0)
			return Monitor::Function_result::COMPLETE;

		if (signal_occurred_during_select())
			return Monitor::Function_result::COMPLETE;

		return Monitor::Function_result::INCOMPLETE;
	};

	Monitor::Result const monitor_result =
		_monitor_ptr->monitor(monitor_fn, timeout_ms);

	select_cb_list().remove(&(*select_cb));

	if (monitor_result == Monitor::Result::TIMEOUT)
		return 0;

	if (signal_occurred_during_select())
		return Errno(EINTR);

	/* not timed out -> results have been stored in select_cb by select_notify_from_kernel() */

	if (readfds)   *readfds   = select_cb->readfds;
	if (writefds)  *writefds  = select_cb->writefds;
	if (exceptfds) *exceptfds = select_cb->exceptfds;

	return select_cb->nready;
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
	fd_set in_readfds, in_writefds, in_exceptfds;

	in_readfds   = readfds;
	in_writefds  = writefds;
	in_exceptfds = exceptfds;

	/* remove potentially enqueued callback from list */
	if (_select_cb->constructed())
		select_cb_list().remove(&(**_select_cb));

	{
		/*
		 * We use the guard directly to atomically check if any descripor is
		 * ready, and insert into select-callback list otherwise.
		 */
		Select_cb_list::Guard guard(select_cb_list());

		int const nready = selscan(nfds,
		                           &in_readfds, &in_writefds, &in_exceptfds,
		                           &readfds, &writefds, &exceptfds);

		/* return if any descripor is ready */
		if (nready)
			return nready;

		/* suspend as we don't have any immediate events */

		_select_cb->construct(nfds, in_readfds, in_writefds, in_exceptfds);

		select_cb_list().unsynchronized_insert(&(**_select_cb));
	}

	struct Missing_call_of_init_select : Exception { };
	if (!_select_ptr)
		throw Missing_call_of_init_select();

	_select_ptr->schedule_select(*this);

	return 0;
}


void Libc::Select_handler_base::dispatch_select()
{
	Libc::select_notify_from_kernel();

	Select_handler_cb &select_cb = *_select_cb;

	if (select_cb->nready == 0) return;

	select_cb_list().remove(&(*select_cb));

	if (_select_ptr)
		_select_ptr->deschedule_select();

	select_ready(select_cb->nready, select_cb->readfds,
	             select_cb->writefds, select_cb->exceptfds);
}


Libc::Select_handler_base::Select_handler_base()
:
	_select_cb((Select_handler_cb*)malloc(sizeof(*_select_cb)))
{ }


Libc::Select_handler_base::~Select_handler_base()
{ }
