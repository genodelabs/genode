/*
 * \brief  File-system node
 * \author Norman Feske
 * \date   2012-04-11
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

			Lock           _lock;
			List<Listener> _listeners;

		public:

			virtual ~Node_base()
			{
				/* propagate event to listeners */
				mark_as_updated();
				notify_listeners();

				while (_listeners.first())
					_listeners.remove(_listeners.first());
			}

			void lock()   { _lock.lock(); }
			void unlock() { _lock.unlock(); }

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
					curr->notify();
			}

			void mark_as_updated()
			{
				for (Listener *curr = _listeners.first(); curr; curr = curr->next())
					curr->mark_as_updated();
			}
	};


	/**
	 * Guard used for properly releasing node locks
	 */
	struct Node_lock_guard
	{
		Node_base *node;

		Node_lock_guard(Node_base *node) : node(node) { node = node; }

		~Node_lock_guard() { node->unlock(); }
	};
}

#endif /* _FILE_SYSTEM__NODE_H_ */
