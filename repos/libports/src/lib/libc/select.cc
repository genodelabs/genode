/*
 * \brief  select() implementation
 * \author Christian Prochaska
 * \author Christian Helmuth
 * \author Emery Hemingway
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
#include <util/reconstructible.h>

/* Libc includes */
#include <libc-plugin/plugin_registry.h>
#include <libc-plugin/plugin.h>
#include <libc/select.h>
#include <stdlib.h>
#include <sys/select.h>
#include <signal.h>

#include "task.h"


namespace Libc {
	struct Select_cb;
	struct Select_cb_list;
}


void (*libc_select_notify)() __attribute__((weak));


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
	Genode::Lock  _mutex;
	Select_cb    *_first = nullptr;

	struct Guard : Genode::Lock::Guard
	{
Select_cb_list *l;

		Guard(Select_cb_list &list) : Genode::Lock::Guard(list._mutex), l(&list) { }
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
static Libc::Select_cb_list select_cb_list;


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

	for (Libc::Plugin *plugin = Libc::plugin_registry()->first();
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
				Genode::error("plugin->select() returned error value ", plugin_nready);
			}
		}
	}

	return nready;
}


/* this function gets called by plugin backends when file descripors become ready */
static void select_notify()
{
	bool resume_all = false;
	fd_set tmp_readfds, tmp_writefds, tmp_exceptfds;

	/* check for each waiting select() function if one of its fds is ready now
	 * and if so, wake all up */

	select_cb_list.for_each([&] (Libc::Select_cb &scb) {
		scb.nready = selscan(scb.nfds,
		                     &scb.readfds, &scb.writefds, &scb.exceptfds,
		                     &tmp_readfds,  &tmp_writefds,  &tmp_exceptfds);
		if (scb.nready > 0) {
			scb.readfds   = tmp_readfds;
			scb.writefds  = tmp_writefds;
			scb.exceptfds = tmp_exceptfds;

			resume_all = true;
		}
	});

	if (resume_all)
		Libc::resume_all();
}


static void print(Genode::Output &output, timeval *tv)
{
	if (!tv) {
		print(output, "nullptr");
	} else {
		print(output, "{");
		print(output, tv->tv_sec);
		print(output, ",");
		print(output, tv->tv_usec);
		print(output, "}");
	}
}


extern "C" int
__attribute__((weak))
_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
        struct timeval *tv)
{
	fd_set in_readfds, in_writefds, in_exceptfds;

	Genode::Constructible<Libc::Select_cb> select_cb;

	/* initialize the select notification function pointer */
	if (!libc_select_notify)
		libc_select_notify = select_notify;

	if (readfds)   in_readfds   = *readfds;   else FD_ZERO(&in_readfds);
	if (writefds)  in_writefds  = *writefds;  else FD_ZERO(&in_writefds);
	if (exceptfds) in_exceptfds = *exceptfds; else FD_ZERO(&in_exceptfds);

	{
		/*
		 * We use the guard directly to atomically check is any descripor is
		 * ready, and insert into select-callback list otherwise.
		 */
		Libc::Select_cb_list::Guard guard(select_cb_list);

		int const nready = selscan(nfds,
		                           &in_readfds, &in_writefds, &in_exceptfds,
		                           readfds, writefds, exceptfds);

		/* return if any descripor is ready */
		if (nready)
			return nready;

		/* return on zero-timeout */
		if (tv && (tv->tv_sec) == 0 && (tv->tv_usec == 0))
			return 0;

		/* suspend as we don't have any immediate events */

		select_cb.construct(nfds, in_readfds, in_writefds, in_exceptfds);

		select_cb_list.unsynchronized_insert(&(*select_cb));
	}

	struct Timeout
	{
		timeval const *_tv;
		bool    const  valid    { _tv != nullptr };
		unsigned long  duration { valid ? _tv->tv_sec*1000 + _tv->tv_usec/1000 : 0UL };

		bool expired() const { return valid && duration == 0; };

		Timeout(timeval *tv) : _tv(tv) { }
	} timeout { tv };

	struct Check : Libc::Suspend_functor {
		struct Timeout  *timeout;
		Libc::Select_cb *select_cb;

		Check(Timeout *timeout, Libc::Select_cb * select_cb)
		: timeout(timeout), select_cb(select_cb) { }

		bool suspend() override {
			return !timeout->expired() && select_cb->nready == 0; }
	} check ( &timeout, &*select_cb );

	while (!timeout.expired() && select_cb->nready == 0)
		timeout.duration = Libc::suspend(check, timeout.duration);

	select_cb_list.remove(&(*select_cb));

	if (timeout.expired())
		return 0;

	/* not timed out -> results have been stored in select_cb by select_notify() */

	if (readfds)   *readfds   = select_cb->readfds;
	if (writefds)  *writefds  = select_cb->writefds;
	if (exceptfds) *exceptfds = select_cb->exceptfds;

	return select_cb->nready;
}


extern "C" int
__attribute__((weak))
select(int nfds, fd_set *readfds, fd_set *writefds,
       fd_set *exceptfds, struct timeval *timeout)
{
	return _select(nfds, readfds, writefds, exceptfds, timeout);
}


extern "C" int
__attribute__((weak))
_pselect(int nfds, fd_set *readfds, fd_set *writefds,
         fd_set *exceptfds, const struct timespec *timeout,
         const sigset_t *sigmask)
{
	struct timeval tv;
	sigset_t origmask;
	int nready;

	if (timeout) {
		tv.tv_usec = timeout->tv_nsec / 1000;
		tv.tv_sec = timeout->tv_sec;
	}

	if (sigmask)
		sigprocmask(SIG_SETMASK, sigmask, &origmask);
	nready = select(nfds, readfds, writefds, exceptfds, &tv);
	if (sigmask)
		sigprocmask(SIG_SETMASK, &origmask, NULL);

	return nready;
}


extern "C" int
__attribute__((weak))
pselect(int nfds, fd_set *readfds, fd_set *writefds,
        fd_set *exceptfds, const struct timespec *timeout,
        const sigset_t *sigmask)
{
	return _pselect(nfds, readfds, writefds, exceptfds, timeout, sigmask);
}


/****************************************
 ** Select handler for libc components **
 ****************************************/

int Libc::Select_handler_base::select(int nfds, fd_set &readfds,
                                      fd_set &writefds, fd_set &exceptfds)
{
	fd_set in_readfds, in_writefds, in_exceptfds;

	/* initialize the select notification function pointer */
	if (!libc_select_notify)
		libc_select_notify = select_notify;

	in_readfds   = readfds;
	in_writefds  = writefds;
	in_exceptfds = exceptfds;

	/* remove potentially enqueued callback from list */
	if (_select_cb->constructed())
		select_cb_list.remove(&(**_select_cb));

	{
		/*
		 * We use the guard directly to atomically check is any descripor is
		 * ready, and insert into select-callback list otherwise.
		 */
		Libc::Select_cb_list::Guard guard(select_cb_list);

		int const nready = selscan(nfds,
		                           &in_readfds, &in_writefds, &in_exceptfds,
		                           &readfds, &writefds, &exceptfds);

		/* return if any descripor is ready */
		if (nready)
			return nready;

		/* suspend as we don't have any immediate events */

		_select_cb->construct(nfds, readfds, writefds, exceptfds);

		select_cb_list.unsynchronized_insert(&(**_select_cb));
	}

	Libc::schedule_select(this);

	return 0;
}


void Libc::Select_handler_base::dispatch_select()
{
	Select_handler_cb &select_cb = *_select_cb;

	if (select_cb->nready == 0) return;

	select_cb_list.remove(&(*select_cb));
	Libc::schedule_select(nullptr);

	select_ready(select_cb->nready, select_cb->readfds,
	             select_cb->writefds, select_cb->exceptfds);
}


Libc::Select_handler_base::Select_handler_base()
:
	_select_cb((Select_handler_cb*)malloc(sizeof(*_select_cb)))
{ }

Libc::Select_handler_base::~Select_handler_base()
{ }
