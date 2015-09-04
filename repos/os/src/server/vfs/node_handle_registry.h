/*
 * \brief  Facility for managing the session-local node-handle namespace
 * \author Norman Feske
 * \date   2012-04-11
 *
 * This file is derived from os/include/file_system/node_handle_registry.h.
 */

#ifndef _VFS__NODE_HANDLE_REGISTRY_H_
#define _VFS__NODE_HANDLE_REGISTRY_H_

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

			Lock mutable _lock;
			Node        *_nodes[MAX_NODE_HANDLES];

			/**
			 * Each open node handle can act as a listener to be informed about
			 * node changes.
			 */
			Listener _listeners[MAX_NODE_HANDLES];

			/**
			 * Mode information is stored here for each open node.
			 */
			enum Mode _modes[MAX_NODE_HANDLES];

			/**
			 * A cache of open nodes shared between sessions.
			 */
			Node_cache &_cache;

			/**
			 * Allocate node handle
			 *
			 * \throw Out_of_node_handles
			 */
			int _alloc(Node *node, Mode mode)
			{
				Lock::Guard guard(_lock);

				for (unsigned i = 0; i < MAX_NODE_HANDLES; i++)
					if (!_nodes[i]) {
						_nodes[i] = node;
						_modes[i] = mode;
						return i;
					}

				throw Out_of_node_handles();
			}

			bool _in_range(int handle) const
			{
				return ((handle >= 0) && (handle < MAX_NODE_HANDLES));
			}

		public:

			Node_handle_registry(Node_cache &cache)
			: _cache(cache)
			{
				for (unsigned i = 0; i < MAX_NODE_HANDLES; i++) {
					_nodes[i] = 0;
					_modes[i] = STAT_ONLY;
				}
			}

			template <typename NODE_TYPE>
			typename Handle_type<NODE_TYPE>::Type alloc(NODE_TYPE *node, Mode mode)
			{
				typedef typename Handle_type<NODE_TYPE>::Type Handle;
				return Handle(_alloc(node, mode));
			}

			/**
			 * Request a file from the cache and allocate a handle.
			 */
			Dir_handle directory(char const *path, bool create)
			{
				Directory *dir = _cache.directory(path, create);
				return alloc(dir, READ_ONLY);
			}

			/**
			 * Request a directory from the cache and allocate a handle.
			 */
			File_handle file(char const *path, Mode mode, bool create)
			{
				File *file = _cache.file(path, mode, create);
				return alloc(file, mode);
			}

			/**
			 * Request a symlink from the cache and allocate a handle.
			 */
			Symlink_handle symlink(char const *path, Mode mode, bool create)
			{
				Symlink *link = _cache.symlink(path, create);
				return alloc(link, mode);
			}

			/**
			 * Request a node from the cache and allocate a handle.
			 */
			Node_handle node(char const *path)
			{
				Node *node = _cache.node(path);
				return alloc(node, STAT_ONLY);
			}

			/**
			 * Release node handle
			 */
			void free(Node_handle handle)
			{
				if (!_in_range(handle.value))
					return;

				Lock::Guard guard(_lock);

				Node *node = dynamic_cast<Node *>(_nodes[handle.value]);
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

				_modes[handle.value] = STAT_ONLY;

				node->unlock();
				_cache.free(node);
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
				if (!_in_range(handle.value))
					throw Invalid_handle();

				Lock::Guard guard(_lock);

				typedef typename Node_type<HANDLE_TYPE>::Type Node;
				Node *node = dynamic_cast<Node *>(_nodes[handle.value]);
				if (!node)
					throw Invalid_handle();

				node->lock();
				return node;
			}

			/**
			 * Lookup a node for reading
			 *
			 * A node in a locked state or a null pointer is returned.
			 */
			Node *lookup_read(Node_handle handle)
			{
				if (!_in_range(handle.value))
					return nullptr;

				Lock::Guard guard(_lock);

				switch (_modes[handle.value]) {
				case READ_ONLY:
				case READ_WRITE:
					break;
				default:
					return nullptr;
				}

				Node *node = dynamic_cast<Node *>(_nodes[handle.value]);
				if (node)
					node->lock();
				return node;
			}

			/**
			 * Lookup a node for writing
			 *
			 * A node in a locked state or a null pointer is returned.
			 */
			Node *lookup_write(Node_handle handle)
			{
				if (!_in_range(handle.value))
					return nullptr;

				Lock::Guard guard(_lock);

				switch (_modes[handle.value]) {
				case WRITE_ONLY:
				case READ_WRITE:
					break;
				default:
					return nullptr;
				}

				Node *node = dynamic_cast<Node *>(_nodes[handle.value]);
				if (node)
					node->lock();
				return node;
			}

			bool refer_to_same_node(Node_handle h1, Node_handle h2) const
			{
				Lock::Guard guard(_lock);

				if (!(_in_range(h1.value) && _in_range(h2.value)))
					throw Invalid_handle();

				return _nodes[h1.value] == _nodes[h2.value];
			}

			/**
			 * Register signal handler to be notified of node changes
			 */
			void sigh(Node_handle handle, Signal_context_capability sigh)
			{
				if (!_in_range(handle.value))
					throw Invalid_handle();

				Lock::Guard guard(_lock);

				Node *node = dynamic_cast<Node *>(_nodes[handle.value]);
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

			/**
			 * Remove a path from the cache.
			 */
			void remove(char const *path) { _cache.remove_path(path); }

			/**
			 * Rename a node in the cache.
			 */
			void rename(char const *from, char const *to) {
				_cache.rename(from, to); }

			/**
			 * Is the node open (present in the cache)?
			 */
			bool is_open(char const *path) {
				return _cache.find(path); }
	};
}

#endif /* _VFS__NODE_HANDLE_REGISTRY_H_ */
