/*
 * \brief  File-system node
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Josef Soentgen
 * \date   2013-11-11
 */

#ifndef _NODE_H_
#define _NODE_H_

/* Genode includes */
#include <util/list.h>
#include <base/lock.h>
#include <base/signal.h>
#include <os/path.h>


namespace File_system {

	typedef Genode::Path<MAX_PATH_LEN> Absolute_path;

	class Listener : public List<Listener>::Element
	{
		private:

			Lock                      _lock;
			Signal_context_capability _sigh;
			bool                      _marked_as_updated;

		public:

			Listener() : _marked_as_updated(false) { }

			Listener(Signal_context_capability sigh)
			: _sigh(sigh), _marked_as_updated(false) { }

			void notify()
			{
				Lock::Guard guard(_lock);

				if (_marked_as_updated && _sigh.valid())
					Signal_transmitter(_sigh).submit();

				_marked_as_updated = false;
			}

			void mark_as_updated()
			{
				Lock::Guard guard(_lock);

				_marked_as_updated = true;
			}

			bool valid() const { return _sigh.valid(); }
	};


	class Node : public List<Node>::Element
	{
		protected:

			unsigned long _inode;
			Absolute_path _name;

		private:

			Lock                _lock;
			List<Listener>      _listeners;

		public:

			Node(char const *name) : _name(name) { }

			virtual ~Node()
			{
				/* propagate event to listeners */
				mark_as_updated();
				notify_listeners();

				while (_listeners.first())
					_listeners.remove(_listeners.first());
			}

			char   const *name()  const { return _name.base(); }

			void lock() { _lock.lock(); }
			void unlock() { _lock.unlock(); }

			virtual size_t read(char *dst, size_t len, seek_off_t) = 0;
			virtual size_t write(char const *src, size_t len, seek_off_t) = 0;

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
		Node &node;

		Node_lock_guard(Node &node) : node(node) { }

		~Node_lock_guard() { node.unlock(); }
	};
}

#endif /* _NODE_H_ */
