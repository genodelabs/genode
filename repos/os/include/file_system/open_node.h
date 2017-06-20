/*
 * \brief  Representation of an open file system node within the component
 * \author Christian Prochaska
 * \date   2017-06-09
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _OPEN_NODE_H_
#define _OPEN_NODE_H_

/* Genode includes */
#include <file_system/listener.h>
#include <file_system_session/file_system_session.h>

namespace File_system {
	/*
 	 * \param NODE    component-specific node type
 	 */
	template <typename NODE> class Open_node;
}

template <typename NODE>
class File_system::Open_node : public File_system::Node
{
	private:

		Genode::Id_space<File_system::Node>::Element _element;

		NODE                                         &_node;
		Genode::Constructible<File_system::Listener>  _listener;

	public:

		Open_node(NODE &node, Genode::Id_space<File_system::Node> &id_space)
		: _element(*this, id_space),
		  _node(node) { }

		~Open_node()
		{
			if (_listener.constructed()) {
				_node.remove_listener(&*_listener);
				_listener.destruct();
			}
		}

		NODE                  &node()     { return _node; }
		File_system::Listener &listener() { return *_listener; }

		Genode::Id_space<File_system::Node>::Id id() { return _element.id(); }

		/**
		 * Register packet stream sink to be notified of node changes
		 */
		void register_notify(File_system::Sink &sink)
		{
			/*
			 * If there was already a handler registered for the node,
			 * remove the old handler.
			 */
			if (_listener.constructed()) {
				_node.remove_listener(&*_listener);
				_listener.destruct();
			}

			/*
			 * Register new handler
			 */
			_listener.construct(sink, id());
			_node.add_listener(&*_listener);
		}
};

#endif /* _OPEN_NODE_H_ */
