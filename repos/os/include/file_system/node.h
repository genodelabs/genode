/*
 * \brief  File-system node
 * \author Norman Feske
 * \date   2012-04-11
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FILE_SYSTEM__NODE_H_
#define _FILE_SYSTEM__NODE_H_

/* Genode includes */
#include <file_system/listener.h>
#include <util/list.h>
#include <base/lock.h>


namespace File_system {

	class Node_base
	{
		private:

			Genode::List<Listener> _listeners { };

			typedef Listener::Version Version;

			Version _curr_version { 0 };

		public:

			virtual ~Node_base()
			{
				/* propagate event to listeners */
				mark_as_updated();
				notify_listeners();

				while (_listeners.first())
					_listeners.remove(_listeners.first());
			}

			void add_listener(Listener *listener)
			{
				_listeners.insert(listener);
			}

			void remove_listener(Listener *listener)
			{
				_listeners.remove(listener);
			}

			void notify_listeners()
			{
				for (Listener *curr = _listeners.first(); curr; curr = curr->next())
					curr->notify(_curr_version);
			}

			void mark_as_updated()
			{
				_curr_version = Version { _curr_version.value + 1 };
			}

			Version curr_version() const { return _curr_version; }
	};
}

#endif /* _FILE_SYSTEM__NODE_H_ */
