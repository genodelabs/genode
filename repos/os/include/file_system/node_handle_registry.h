/*
 * \brief  Facility for managing the session-local node-handle namespace
 * \author Norman Feske
 * \date   2012-04-11
 */

#ifndef _FILE_SYSTEM__NODE_HANDLE_REGISTRY_H_
#define _FILE_SYSTEM__NODE_HANDLE_REGISTRY_H_

#include <file_system/node.h>

namespace File_system {

	class Node;
	class Directory;
	class File;
	class Symlink;

	/**
	 * Type trait for determining the node type for a given handle type
	 */
	template<typename T> struct Node_type;
	template<> struct Node_type<Node_handle>    { typedef Node      Type; };
	template<> struct Node_type<Dir_handle>     { typedef Directory Type; };
	template<> struct Node_type<File_handle>    { typedef File      Type; };
	template<> struct Node_type<Symlink_handle> { typedef Symlink   Type; };


	/**
	 * Type trait for determining the handle type for a given node type
	 */
	template<typename T> struct Handle_type;
	template<> struct Handle_type<Node>      { typedef Node_handle    Type; };
	template<> struct Handle_type<Directory> { typedef Dir_handle     Type; };
	template<> struct Handle_type<File>      { typedef File_handle    Type; };
	template<> struct Handle_type<Symlink>   { typedef Symlink_handle Type; };


	class Node_handle_registry
	{
		private:

			/* maximum number of open nodes per session */
			enum { MAX_NODE_HANDLES = 128U };

			Genode::Lock mutable _lock;

			Node_base *_nodes[MAX_NODE_HANDLES];

			/**
			 * Each open node handle can act as a listener to be informed about
			 * node changes.
			 */
			Listener _listeners[MAX_NODE_HANDLES];

			/**
			 * Allocate node handle
			 *
			 * \throw Out_of_metadata
			 */
			int _alloc(Node_base *node)
			{
				Genode::Lock::Guard guard(_lock);

				for (unsigned i = 0; i < MAX_NODE_HANDLES; i++)
					if (!_nodes[i]) {
						_nodes[i] = node;
						return i;
					}

				throw Out_of_metadata();
			}

			bool _in_range(int handle) const
			{
				return ((handle >= 0) && (handle < MAX_NODE_HANDLES));
			}

		public:

			Node_handle_registry()
			{
				for (unsigned i = 0; i < MAX_NODE_HANDLES; i++)
					_nodes[i] = 0;
			}

			template <typename NODE_TYPE>
			typename Handle_type<NODE_TYPE>::Type alloc(NODE_TYPE *node)
			{
				typedef typename Handle_type<NODE_TYPE>::Type Handle;
				return Handle(_alloc(node));
			}

			/**
			 * Release node handle
			 */
			void free(Node_handle handle)
			{
				Genode::Lock::Guard guard(_lock);

				if (!_in_range(handle.value))
					return;

				/*
				 * Notify listeners about the changed file.
				 */
				Node_base *node = dynamic_cast<Node_base *>(_nodes[handle.value]);
				if (!node) { return; }

				node->lock();
				node->notify_listeners();

				/*
				 * De-allocate handle
				 */
				Listener &listener = _listeners[handle.value];

				if (listener.valid())
					node->remove_listener(&listener);

				_nodes[handle.value] = 0;
				listener = Listener();

				node->unlock();
			}

			/**
			 * Lookup node using its handle as key
			 *
			 * \throw Invalid_handle
			 */
			template <typename HANDLE_TYPE>
			typename Node_type<HANDLE_TYPE>::Type *lookup(HANDLE_TYPE handle)
			{
				Genode::Lock::Guard guard(_lock);

				if (!_in_range(handle.value))
					throw Invalid_handle();

				typedef typename Node_type<HANDLE_TYPE>::Type Node;
				Node *node = dynamic_cast<Node *>(_nodes[handle.value]);
				if (!node)
					throw Invalid_handle();

				return node;
			}

			/**
			 * Lookup node using its handle as key
			 *
			 * The node returned by this function is in a locked state.
			 *
			 * \throw Invalid_handle
			 */
			template <typename HANDLE_TYPE>
			typename Node_type<HANDLE_TYPE>::Type *lookup_and_lock(HANDLE_TYPE handle)
			{
				Genode::Lock::Guard guard(_lock);

				if (!_in_range(handle.value))
					throw Invalid_handle();

				typedef typename Node_type<HANDLE_TYPE>::Type Node_base;
				Node_base *node = dynamic_cast<Node_base *>(_nodes[handle.value]);
				if (!node)
					throw Invalid_handle();

				node->lock();
				return node;
			}

			bool refer_to_same_node(Node_handle h1, Node_handle h2) const
			{
				Genode::Lock::Guard guard(_lock);

				if (!_in_range(h1.value) || !_in_range(h2.value))
					throw Invalid_handle();

				return _nodes[h1.value] == _nodes[h2.value];
			}

			/**
			 * Register signal handler to be notified of node changes
			 */
			void sigh(Node_handle handle, Genode::Signal_context_capability sigh)
			{
				Genode::Lock::Guard guard(_lock);

				if (!_in_range(handle.value))
					throw Invalid_handle();

				Node_base *node = dynamic_cast<Node_base *>(_nodes[handle.value]);
				if (!node)
					throw Invalid_handle();

				node->lock();
				Node_lock_guard node_lock_guard(node);

				Listener &listener = _listeners[handle.value];

				/*
				 * If there was already a handler registered for the node,
				 * remove the old handler.
				 */
				if (listener.valid())
					node->remove_listener(&listener);

				/*
				 * Register new handler
				 */
				listener = Listener(sigh);
				node->add_listener(&listener);
			}
	};
}

#endif /* _FILE_SYSTEM__NODE_HANDLE_REGISTRY_H_ */
