/*
 * \brief  inotify handling for underlying file system.
 * \author Pirmin Duss
 * \date   2020-06-17
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 * Copyright (C) 2020-2021 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include "base/exception.h"
#include <base/log.h>
#include "base/signal.h"
#include "util/string.h"

/* libc includes */
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>      /* strerror */
#include <sys/inotify.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

/* local includes */
#include "fd_set.h"
#include "notifier.h"
#include "watch.h"


namespace Lx_fs
{
	enum {
		STACK_SIZE             = 8 * 1024,
		EVENT_SIZE             = (sizeof (struct inotify_event)),
		EVENT_BUF_LEN          = (1024 * (EVENT_SIZE + NAME_MAX + 1)),
		PARALLEL_NOTIFICATIONS = 4,
		INOTIFY_WATCH_MASK     = IN_CLOSE_WRITE |   /* writtable file was closed */
		                         IN_MOVED_TO    |   /* file was moved to Y */
		                         IN_MOVED_FROM  |   /* file was moved from X */
		                         IN_CREATE      |   /* subfile was created */
		                         IN_DELETE      |   /* subfile was deleted */
		                         IN_IGNORED,        /* file was ignored */
	};

	struct Libc_signal_thread;
}

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

/* do not leak internal function in to global namespace */
namespace
{
	using namespace Lx_fs;

	::uint64_t timestamp_us()
	{
		struct timeval ts { 0, 0 };
		gettimeofday(&ts, nullptr);

		return (ts.tv_sec * 1'000'000) + ts.tv_usec;
	}


	template <typename T, typename Allocator>
	T *remove_from_list(Genode::List<T> &list, T *node, Allocator &alloc)
	{
		T *next { node->next() };
		list.remove(node);
		destroy(alloc, node);
		return next;
	}


	bool is_dir(char const *path)
	{
		struct stat s;
		int ret = lstat(path, &s);
		if (ret == -1)
			return false;

		if (S_ISDIR(s.st_mode))
			return true;

		return false;
	}


	Path_string get_directory(Path_string const &path)
	{
		Path_string directory;
		if (is_dir(path.string())) {
			// make sure there is a '/' at the end of the path
			if (path.string()[path.length() - 2] == '/')
				directory = path;
			else
				directory = Path_string { path, '/' };
		} else {
			size_t pos = 0;
			for (size_t i = 0; i < path.length() - 1; ++i) {
				if (path.string()[i] == '/')
					pos = i;
			}
			directory = Genode::Cstring { path.string(), pos + 1 };
		}

		return directory;
	}


	Path_string get_filename(Path_string const &path)
	{
		Path_string filename;
		if (is_dir(path.string()))
			return { };

		size_t pos = 0;
		for (size_t i = 0; i < path.length() - 1; ++i) {
			if (path.string()[i] == '/')
				pos = i;
		}

		/* if '/' is the last symbol we do not need a filename */
		if (pos != path.length() - 2)
			filename = Genode::Cstring { path.string() + pos + 1 };

		return filename;
	}

}  /* anonymous namespace */


Lx_fs::Os_path::Os_path(const char *fullname)
:
	full_path { fullname },
	directory { get_directory(full_path) },
	filename  { get_filename(full_path) }
{ }


bool Lx_fs::Notifier::_watched(char const *path) const
{
	for (Watches_list_element const *e = _watched_nodes.first(); e != nullptr; e = e->next()) {
		if (e->path.full_path == path)
			return true;
	}

	return false;
}


void Lx_fs::Notifier::_add_to_watched(char const *fullname)
{
	Os_path path { fullname };
	for (Watches_list_element *e = _watched_nodes.first(); e != nullptr; e = e->next()) {
		if (e->path.directory == path.directory) {
			Watches_list_element *elem { new (_heap) Watches_list_element { e->watch_fd, path } };
			_watched_nodes.insert(elem);
			return;
		}
	}

	auto watch_fd { inotify_add_watch(_fd, path.directory.string(), INOTIFY_WATCH_MASK) };

	if (watch_fd > 0) {
		Watches_list_element *elem { new (_heap) Watches_list_element { watch_fd, path } };
		_watched_nodes.insert(elem);
	}
}


int Lx_fs::Notifier::_add_node(char const *path, Watch_node &node)
{
	for (Watches_list_element *e = _watched_nodes.first(); e != nullptr; e = e->next()) {
		if (e->path.full_path == path) {
			Single_watch_list_element *c { new (_heap) Single_watch_list_element { node } };
			e->add_node(c);
			return e->watch_fd;
		}
	};

	throw File_system::Lookup_failed { };
}


void Lx_fs::Notifier::_add_notify(Watch_node &node)
{
	Mutex::Guard  guard { _notify_queue_mutex };
	for (auto const *e = _notify_queue.first(); e != nullptr; e = e->next()) {
		if (&e->node == &node) {
			return;
		}
	}

	auto *entry { new (_heap) Single_watch_list_element { node } };
	_notify_queue.insert(entry);
}


void Lx_fs::Notifier::_process_notify()
{
	Mutex::Guard  guard { _notify_queue_mutex };

	/**
	 * limit amount of watch events sent at the same time,
	 * to prevent an overflow of the packet ack queue of the
	 * File_system session.
	 */
	int cnt { 0 };
	Single_watch_list_element *e { _notify_queue.first() };
	for (; e != nullptr && cnt < PARALLEL_NOTIFICATIONS; ++cnt) {
		e->node.notify_handler().local_submit();
		e = remove_from_list(_notify_queue, e, _heap);
	}
}


Lx_fs::Notifier::Notifier(Env &env)
:
	Thread { env, "inotify", STACK_SIZE },
	_env   { env }
{
	_fd = inotify_init();

	if (0 > _fd)
		throw Init_notify_failed { };

	start();
}


Lx_fs::Notifier::~Notifier()
{
	/* do not notify the elements */
	for (auto *e = _notify_queue.first(); e != nullptr; ) {
		e = remove_from_list(_notify_queue, e, _heap);
	}

	for (auto *e = _watched_nodes.first(); e != nullptr; ) {
		e = remove_from_list(_watched_nodes, e, _heap);
	}

	close(_fd);
}


Lx_fs::Notifier::Watches_list_element *Lx_fs::Notifier::_remove_node(Watches_list_element *node)
{
	int    watch_fd   { node->watch_fd };
	Watches_list_element *next       { remove_from_list(_watched_nodes, node, _heap) };
	bool   nodes_left { false };

	Watches_list_element const *e { _watched_nodes.first() };
	for (; e != nullptr && !nodes_left; e = e->next()) {
		nodes_left = e->watch_fd == watch_fd;
	}

	if (!nodes_left) {
		inotify_rm_watch(_fd, watch_fd);
	}

	return next;
}


void Lx_fs::Notifier::_handle_modify_file(inotify_event *event)
{
	for (Watches_list_element *e = _watched_nodes.first(); e != nullptr; e = e->next()) {
		if (e->watch_fd == event->wd && (e->path.filename == event->name || e->path.is_dir())) {
			e->notify_all([this] (Watch_node &node) {
				_add_notify(node);
			});
		}
	}
}


void Lx_fs::Notifier::_remove_empty_watches()
{
	for (Watches_list_element *e = _watched_nodes.first(); e != nullptr; ) {
		if (e->empty()) {
			e = _remove_node(e);
		} else {
			e = e->next();
		}
	}
}


void Lx_fs::Notifier::entry()
{
	enum {
		SELECT_TIMEOUT_US   = 5000,    /* 5 milliseconds */
	};

	auto                  notify_ts             { timestamp_us() };
	bool                  pending_notify        { false };
	struct inotify_event *event                 { nullptr };
	char                  buffer[EVENT_BUF_LEN] { 0 };
	while (true) {

		int            num_select { 0 };
		Fd_set         fds        { _fd };
		struct timeval tv         { .tv_sec  = 0, .tv_usec = SELECT_TIMEOUT_US };

		/*
		 * if no notifications are pending we wait until a inotify event occurs,
		 * otherwise we wait with a timeout on which we deliver some notifications
		 */
		{
			Mutex::Guard  guard { _notify_queue_mutex };
			pending_notify = _notify_queue.first() == nullptr;
		}
		if (pending_notify)
			num_select = select(fds.nfds(), fds.fdset(), nullptr, nullptr, nullptr);
		else
			num_select = select(fds.nfds(), fds.fdset(), nullptr, nullptr, &tv);

		/*
		 * select failed
		 */
		if (num_select < 0) {
			error("select on Linux event queue failed error=", Cstring { strerror(errno) });
			continue;
		}

		/*
		 * select timed out, check if notifications are pending and send a bunch if neccessary
		 */
		if (num_select == 0) {
			_process_notify();
			notify_ts = timestamp_us();
			continue;
		}

		/*
		 * data is ready to be read from the inotify file descriptor
		 */
		ssize_t const length { read(_fd, buffer, EVENT_BUF_LEN) };
		ssize_t       pos    { 0 };

		while (pos < length) {
			event = reinterpret_cast<struct inotify_event*>(&buffer[pos]);

			/*
			 * one of the registered events was triggered for
			 * one of the watched files/directories
			 */
			if (event->mask & (INOTIFY_WATCH_MASK)) {
				Mutex::Guard guard { _watched_nodes_mutex };
				_handle_modify_file(event);
			}

			/* Linux kernel watch queue overflow */
			else if (event->mask & IN_Q_OVERFLOW) {
				error("Linux event queue overflow");
				break;
			}

			pos += EVENT_SIZE + event->len;
		}

		/*
		 * Ensure that notifications are sent even when a lot of changes
		 * on the file system prevent the timout of select from triggering.
		 */
		auto const delta { timestamp_us() - notify_ts };
		if (delta > SELECT_TIMEOUT_US) {
			_process_notify();
			notify_ts = timestamp_us();
		}
	}
}


int Lx_fs::Notifier::add_watch(const char* path, Watch_node &node)
{
	{
		Mutex::Guard guard { _watched_nodes_mutex };

		if (!_watched(path)) {
			_add_to_watched(path);
		}
	}

	return _add_node(path, node);
}


void Lx_fs::Notifier::remove_watch(char const *path, Watch_node &node)
{
	{
		Mutex::Guard  guard { _notify_queue_mutex };
		auto         *e     { _notify_queue.first() };
		while (e != nullptr) {
			if (&e->node == &node) {
				auto *tmp { e };
				e = e->next();
				_notify_queue.remove(tmp);
				destroy(_heap, tmp);
			} else {
				e = e->next();
			}
		}
	}

	{
		Mutex::Guard guard { _watched_nodes_mutex };

		for (Watches_list_element *e = _watched_nodes.first(); e != nullptr; e = e->next()) {
			if (e->path.full_path == path) {
				e->remove_node(_heap, node);
			}
		}

		_remove_empty_watches();
	}
}
