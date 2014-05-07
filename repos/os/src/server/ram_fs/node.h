/*
 * \brief  File-system node
 * \author Norman Feske
 * \date   2012-04-11
 */

#ifndef _NODE_H_
#define _NODE_H_

/* Genode includes */
#include <util/list.h>
#include <base/lock.h>
#include <base/signal.h>

namespace File_system {

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
		public:

			typedef char Name[128];

		private:

			Lock                _lock;
			int                 _ref_count;
			Name                _name;
			unsigned long const _inode;
			List<Listener>      _listeners;
			bool                _modified;

			/**
			 * Generate unique inode number
			 */
			static unsigned long _unique_inode()
			{
				static unsigned long inode_count;
				return ++inode_count;
			}

		public:

			Node()
			: _ref_count(0), _inode(_unique_inode()), _modified(false)
			{ _name[0] = 0; }

			virtual ~Node()
			{
				/* propagate event to listeners */
				mark_as_updated();
				notify_listeners();

				while (_listeners.first())
					_listeners.remove(_listeners.first());
			}

			unsigned long inode() const { return _inode; }
			char   const *name()  const { return _name; }

			/**
			 * Assign name
			 */
			void name(char const *name) { strncpy(_name, name, sizeof(_name)); }

			void lock()   { _lock.lock(); }
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
