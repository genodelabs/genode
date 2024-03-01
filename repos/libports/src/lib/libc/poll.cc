/*
 * \brief  poll() implementation
 * \author Josef Soentgen
 * \author Christian Helmuth
 * \author Emery Hemingway
 * \date   2012-07-12
 */

/*
 * Copyright (C) 2010-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Libc includes */
#include <sys/poll.h>

/* internal includes */
#include <internal/plugin_registry.h>
#include <internal/plugin.h>
#include <internal/errno.h>
#include <internal/file.h>
#include <internal/init.h>
#include <internal/monitor.h>
#include <internal/signal.h>

using namespace Libc;


static Monitor      *_monitor_ptr;
static Libc::Signal *_signal_ptr;

void Libc::init_poll(Signal &signal, Monitor &monitor)
{
	_signal_ptr  = &signal;
	_monitor_ptr = &monitor;
}


static int poll_plugins(Plugin::Pollfd pollfds[], int nfds)
{
	int nready = 0;

	for (Plugin *plugin = plugin_registry()->first();
	     plugin;
	     plugin = plugin->next()) {

		if (!plugin->supports_poll())
			continue;

		/* count the number of pollfds for this plugin */

		int plugin_nfds = 0;

		for (int pollfd_index = 0; pollfd_index < nfds; pollfd_index++)
			if (pollfds[pollfd_index].fdo->plugin == plugin)
				plugin_nfds++;

		if (plugin_nfds == 0)
			continue;

		/*
		 * Copy the pollfds belonging to this plugin to a plugin-specific
		 * array. 'revents' still points into the original structure.
		 */

		Plugin::Pollfd plugin_pollfds[plugin_nfds];

		for (int pollfd_index = 0, plugin_pollfd_index = 0;
		     pollfd_index < nfds;
		     pollfd_index++) {

			if (pollfds[pollfd_index].fdo->plugin == plugin) {
				plugin_pollfds[plugin_pollfd_index] = pollfds[pollfd_index];
				plugin_pollfd_index++;
			}
		}

		int plugin_nready = plugin->poll(plugin_pollfds, plugin_nfds);

		if (plugin_nready < 0)
			return plugin_nready;

		nready += plugin_nready;
	}

	return nready;
}


extern "C" int
__attribute__((weak))
poll(struct pollfd pollfds[], nfds_t nfds, int timeout_ms)
{
	/*
	 * Look up the file descriptor objects early-on to reduce repeated
	 * overhead.
	 */

	Plugin::Pollfd plugins_pollfds[nfds];

	for (nfds_t pollfd_index = 0; pollfd_index < nfds; pollfd_index++) {
		pollfds[pollfd_index].revents = 0;
		plugins_pollfds[pollfd_index].fdo =
			file_descriptor_allocator()->find_by_libc_fd(pollfds[pollfd_index].fd);
		plugins_pollfds[pollfd_index].events = pollfds[pollfd_index].events;
		plugins_pollfds[pollfd_index].revents = &pollfds[pollfd_index].revents;
	}

	int nready = poll_plugins(plugins_pollfds, nfds);

	/* return if any descriptor is ready or an error occurred */
	if (nready != 0)
		return nready;

	/* return on zero-timeout */
	if (timeout_ms == 0)
		return 0;

	/* convert infinite timeout to monitor interface */
	if (timeout_ms < 0)
		timeout_ms = 0;

	if (!_monitor_ptr || !_signal_ptr) {
		struct Missing_call_of_init_poll : Exception { };
		throw Missing_call_of_init_poll();
	}

	unsigned const orig_signal_count = _signal_ptr->count();

	auto signal_occurred_during_poll = [&] ()
	{
		return (_signal_ptr->count() != orig_signal_count);
	};

	auto monitor_fn = [&] ()
	{
		nready = poll_plugins(plugins_pollfds, nfds);

		if (nready != 0)
			return Monitor::Function_result::COMPLETE;

		if (signal_occurred_during_poll())
			return Monitor::Function_result::COMPLETE;

		return Monitor::Function_result::INCOMPLETE;
	};

	Monitor::Result const monitor_result =
		_monitor_ptr->monitor(monitor_fn, timeout_ms);

	if (monitor_result == Monitor::Result::TIMEOUT)
		return 0;

	if (signal_occurred_during_poll())
		return Errno(EINTR);

	return nready;
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
	int timeout_ms = timeout ?
	                 (timeout->tv_sec * 1000 + timeout->tv_nsec / 1000000) :
	                 -1;
	return poll(fds, nfds, timeout_ms);
}


extern "C" __attribute__((weak, alias("ppoll")))
int __sys_ppoll(struct pollfd fds[], nfds_t nfds,
                const struct timespec *timeout,
                const sigset_t*);
