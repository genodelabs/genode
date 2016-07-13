/*
 * \brief  select() implementation
 * \author Christian Prochaska
 * \date   2010-01-21
 *
 * the 'select()' implementation is partially based on the lwip version as
 * implemented in 'src/api/sockets.c'
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/log.h>
#include <os/timed_semaphore.h>

#include <libc-plugin/plugin_registry.h>
#include <libc-plugin/plugin.h>

#include <sys/select.h>
#include <signal.h>

using namespace Libc;


void (*libc_select_notify)() __attribute__((weak));


/** Description for a task waiting in select */
struct libc_select_cb
{
	struct libc_select_cb *next;
	int nfds;
	int nready;
	fd_set readset;
	fd_set writeset;
	fd_set exceptset;
	/** don't signal the same semaphore twice: set to 1 when signalled */
	int sem_signalled;
	/** semaphore to wake up a task waiting for select */
	Timed_semaphore *sem;
};


/** The global list of tasks waiting for select */
static struct libc_select_cb *select_cb_list;


static Genode::Lock &select_cb_list_lock()
{
	static Genode::Lock _select_cb_list_lock;
	return _select_cb_list_lock;
}


/* poll plugin select() functions */
/* input fds may not be NULL */
static int selscan(int nfds, fd_set *in_readfds, fd_set *in_writefds,
                   fd_set *in_exceptfds, fd_set *out_readfds,
                   fd_set *out_writefds, fd_set *out_exceptfds)
{
	int nready = 0;

	 /* zero timeout for polling of the plugins' select() functions */
	struct timeval tv_0 = {0, 0};

	/* temporary fd sets that are passed to the plugins */
	fd_set plugin_readfds;
	fd_set plugin_writefds;
	fd_set plugin_exceptfds;
	int plugin_nready;

	if (out_readfds)
		FD_ZERO(out_readfds);
	if (out_writefds)
		FD_ZERO(out_writefds);
	if (out_exceptfds)
		FD_ZERO(out_exceptfds);

	for (Plugin *plugin = plugin_registry()->first(); plugin; plugin = plugin->next()) {
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
	struct libc_select_cb *scb;
	int nready;
	fd_set tmp_readfds, tmp_writefds, tmp_exceptfds;

	/* check for each waiting select() function if one of its fds is ready now
	 * and if so, wake this select() function up */
	while (1) {
		select_cb_list_lock().lock();
		for (scb = select_cb_list; scb; scb = scb->next) {
			if (scb->sem_signalled == 0) {
				FD_ZERO(&tmp_readfds);
				FD_ZERO(&tmp_writefds);
				FD_ZERO(&tmp_exceptfds);
				nready = selscan(scb->nfds, &scb->readset, &scb->writeset,
			                     &scb->exceptset, &tmp_readfds, &tmp_writefds,
			                     &tmp_exceptfds);
				if (nready > 0)
					break;
			}
		}

		if (scb) {
			scb->sem_signalled = 1;
			scb->nready = nready;
			scb->readset = tmp_readfds;
			scb->writeset = tmp_writefds;
			scb->exceptset = tmp_exceptfds;
			scb->sem->up();
			select_cb_list_lock().unlock();
		} else {
			select_cb_list_lock().unlock();
			break;
		}
	}
}


extern "C" int
__attribute__((weak))
_select(int nfds, fd_set *readfds, fd_set *writefds,
        fd_set *exceptfds, struct timeval *timeout)
{
	int nready;
	fd_set in_readfds, in_writefds, in_exceptfds;
	Genode::Alarm::Time msectimeout;
	struct libc_select_cb select_cb;
	struct libc_select_cb *p_selcb;
	bool timed_out = false;

	/* initialize the select notification function pointer */
	if (!libc_select_notify)
		libc_select_notify = select_notify;

	/* Protect ourselves searching through the list */
	select_cb_list_lock().lock();

	if (readfds)
		in_readfds = *readfds;
	else
		FD_ZERO(&in_readfds);
	if (writefds)
		in_writefds = *writefds;
	else
		FD_ZERO(&in_writefds);
	if (exceptfds)
		in_exceptfds = *exceptfds;
	else
		FD_ZERO(&in_exceptfds);

	/* Go through each socket in each list to count number of sockets which
	   currently match */
	nready = selscan(nfds, &in_readfds, &in_writefds, &in_exceptfds, readfds, writefds, exceptfds);

	/* If we don't have any current events, then suspend if we are supposed to */
	if (!nready) {

		if (timeout && (timeout->tv_sec) == 0 && (timeout->tv_usec == 0)) {
			select_cb_list_lock().unlock();
			if (readfds)
				FD_ZERO(readfds);
			if (writefds)
				FD_ZERO(writefds);
			if (exceptfds)
				FD_ZERO(exceptfds);
			return 0;
		}

		/* add our semaphore to list */
		/* We don't actually need any dynamic memory. Our entry on the
		 * list is only valid while we are in this function, so it's ok
		 * to use local variables */
		select_cb.nfds = nfds;
		select_cb.readset = in_readfds;
		select_cb.writeset = in_writefds;
		select_cb.exceptset = in_exceptfds;
		select_cb.sem_signalled = 0;
		select_cb.sem = new (env()->heap()) Timed_semaphore(0);
		/* Note that we are still protected */
		/* Put this select_cb on top of list */
		select_cb.next = select_cb_list;
		select_cb_list = &select_cb;

		/* Now we can safely unprotect */
		select_cb_list_lock().unlock();

		/* Now just wait to be woken */
		if (!timeout) {
			/* Wait forever */
			select_cb.sem->down();
		} else {
			msectimeout =  ((timeout->tv_sec * 1000) + ((timeout->tv_usec + 500)/1000));
			try {
				select_cb.sem->down(msectimeout);
			} catch (Timeout_exception) {
				timed_out = true;
			}
		}

		/* Take us off the list */
		select_cb_list_lock().lock();

		if (select_cb_list == &select_cb)
			select_cb_list = select_cb.next;
		else
			for (p_selcb = select_cb_list; p_selcb; p_selcb = p_selcb->next) {
				if (p_selcb->next == &select_cb) {
					p_selcb->next = select_cb.next;
					break;
				}
			}

		select_cb_list_lock().unlock();

		destroy(env()->heap(), select_cb.sem);

		if (timed_out)  {
			if (readfds)
				FD_ZERO(readfds);
			if (writefds)
				FD_ZERO(writefds);
			if (exceptfds)
				FD_ZERO(exceptfds);
			return 0;
		}

		/* not timed out -> results have been stored in select_cb by select_notify() */
		nready = select_cb.nready;
		if (readfds)
			*readfds = select_cb.readset;
		if (writefds)
			*writefds = select_cb.writeset;
		if (exceptfds)
			*exceptfds = select_cb.exceptset;
	} else
		select_cb_list_lock().unlock();

	return nready;
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
