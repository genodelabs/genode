/*
 * \brief  inotify handling for underlying file system.
 * \author Pirmin Duss
 * \date   2020-06-17
 */

/*
 * Copyright (C) 2020-2021 Genode Labs GmbH
 * Copyright (C) 2020-2021 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOTIFIER_H_
#define _NOTIFIER_H_

/* Genode includes */
#include <base/exception.h>
#include <base/heap.h>
#include <base/mutex.h>
#include <base/signal.h>
#include <base/thread.h>
#include <file_system/listener.h>
#include <os/path.h>
#include <util/list.h>

/* libc includes */
#include <sys/inotify.h>

/* local includes */
#include "lx_util.h"


namespace Lx_fs
{
	using namespace Genode;

	using Path_string = Genode::String<File_system::MAX_PATH_LEN>;

	class Init_notify_failed : public Genode::Exception { };

	enum { MAX_PATH_SIZE = 1024 };
	struct Os_path;

	class Notifier;

	/* forward decalaration */
	class Watch_node;
}


/*
 * full_path is always a concatenation of directory and filename
 */
struct Lx_fs::Os_path
{
	Path_string const full_path;
	Path_string const directory;    /* always ends with '/' */
	Path_string const filename;

	Os_path(char const *path);

	bool is_dir() const { return filename.length() == 0; }

};


class Lx_fs::Notifier final : public Thread
{
	private:

		struct Single_watch_list_element : public Genode::List<Single_watch_list_element>::Element
		{
			Watch_node &node;

			Single_watch_list_element(Watch_node &node) : node { node } {}
		};

		struct Watches_list_element : public Genode::List<Watches_list_element>::Element
		{
			private:

				List<Single_watch_list_element> _nodes { };

			public:

				int const     watch_fd;
				Os_path const path;

				Watches_list_element(int const watch_fd, Os_path const &path)
				:
					watch_fd { watch_fd }, path { path }
				{ }

				~Watches_list_element() = default;

				bool empty() const { return _nodes.first() == nullptr; }

				void add_node(Single_watch_list_element *cap_entry)
				{
					_nodes.insert(cap_entry);
				}

				template <typename FN>
				void notify_all(FN const &fn)
				{
					for (Single_watch_list_element *e=_nodes.first(); e!=nullptr; e=e->next()) {
						fn(e->node);
					}
				}

				void remove_node(Allocator &alloc, Watch_node &node)
				{
					for (Single_watch_list_element *e=_nodes.first(); e!=nullptr; e=e->next()) {
						if (&e->node == &node) {
							_nodes.remove(e);
							destroy(alloc, e);
							return;
						}
					}
				}
		};

		Env                             &_env;
		Heap                             _heap { _env.ram(), _env.rm() };
		int                              _fd { -1 };
		List<Watches_list_element>       _watched_nodes { };
		Mutex                            _watched_nodes_mutex { };
		List<Single_watch_list_element>  _notify_queue { };
		Mutex                            _notify_queue_mutex { };

		void entry() override;

		void _add_notify(Watch_node &node);
		void _process_notify();
		void _handle_removed_file(inotify_event *event);
		void _handle_modify_file(inotify_event *event);
		void _remove_empty_watches();
		void _print_watches_list();

		bool _watched(char const *path) const;
		void _add_to_watched(char const *path);
		int _add_node(char const *path, Watch_node &node);
		Watches_list_element *_remove_node(Watches_list_element *node);

	public:

		Notifier(Env &env);
		~Notifier();

		int add_watch(const char *path, Watch_node& node);
		void remove_watch(char const *path, Watch_node &node);
};

#endif /* _NOTIFIER_H_ */
