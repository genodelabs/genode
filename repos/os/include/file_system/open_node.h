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

		Genode::Weak_ptr<NODE>                       _node;
		Genode::Constructible<File_system::Listener> _listener { };

		Listener::Version const _version_when_opened { _node_version(_node) };

		/*
		 * Flag to track whether the underlying file-system node was
		 * modified via this 'Open_node'. That is, if closing the 'Open_node'
		 * should notify listeners of the file.
		 */
		bool _was_written = false;

		static Listener::Version const _node_version(Genode::Weak_ptr<NODE> node)
		{
			Genode::Locked_ptr<NODE> locked_node { node };

			if (locked_node.valid())
				return locked_node->curr_version();
			else
				return Listener::Version { 0 };
		}

	public:

		Open_node(Genode::Weak_ptr<NODE> node,
		          Genode::Id_space<File_system::Node> &id_space)
		: _element(*this, id_space), _node(node) { }

		~Open_node()
		{
			Genode::Locked_ptr<NODE> node { _node };

			if (_listener.constructed()) {
				if (node.valid())
					node->remove_listener(&*_listener);
				_listener.destruct();
			}

			/*
			 * Notify remaining listeners about the changed file
			 */
			if (_was_written && node.valid()) {
				node->notify_listeners();
			}
		}

		Genode::Weak_ptr<NODE>&node()     { return _node; }
		File_system::Listener &listener() { return *_listener; }

		Genode::Id_space<File_system::Node>::Id id() { return _element.id(); }

		/**
		 * Register packet stream sink to be notified of node changes
		 */
		void register_notify(File_system::Sink &sink)
		{
			Genode::Locked_ptr<NODE> node { _node };

			/*
			 * If there was already a handler registered for the node,
			 * remove the old handler.
			 */
			if (_listener.constructed()) {
				if (node.valid())
					node->remove_listener(&*_listener);
				_listener.destruct();
			}

			/*
			 * Register new handler
			 */
			if (node.valid()) {
				_listener.construct(sink, id(), _version_when_opened);
				node->add_listener(&*_listener);
			}
		}

		void   mark_as_written() { _was_written = true;  }
		void unmark_as_written() { _was_written = false; }
};

#endif /* _OPEN_NODE_H_ */
