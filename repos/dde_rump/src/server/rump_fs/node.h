/*
 * \brief  File-system node
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2013-11-11
 */

/*
 * Copyright (C) 2013-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NODE_H_
#define _NODE_H_

/* Genode includes */
#include <util/list.h>
#include <base/signal.h>


namespace File_system {
	class Listener;
	class Node;
}

class File_system::Listener : public List<Listener>::Element
{
	private:

		Signal_context_capability _sigh;
		bool                      _marked_as_updated;

	public:

		Listener() : _marked_as_updated(false) { }

		Listener(Signal_context_capability sigh)
		: _sigh(sigh), _marked_as_updated(false) { }

		void notify()
		{
			if (_marked_as_updated && _sigh.valid())
				Signal_transmitter(_sigh).submit();

			_marked_as_updated = false;
		}

		void mark_as_updated()
		{
			_marked_as_updated = true;
		}

		bool valid() const { return _sigh.valid(); }
};


class File_system::Node : public List<Node>::Element
{
	public:

		typedef char Name[128];

	private:

		Name                _name;
		unsigned long const _inode;

		List<Listener>      _listeners;

	public:

		Node(unsigned long inode) : _inode(inode) { _name[0] = 0; }

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

#endif /* _NODE_H_ */
