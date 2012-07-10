/*
 * \brief  Facility for managing the session-local node-handle namespace
 * \author Norman Feske
 * \date   2012-04-11
 */

#ifndef _NODE_HANDLE_REGISTRY_H_
#define _NODE_HANDLE_REGISTRY_H_

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

			Node *_nodes[MAX_NODE_HANDLES];

			/**
			 * Allocate node handle
			 *
			 * \throw Out_of_node_handles
			 */
			int _alloc(Node *node)
			{
				Lock::Guard guard(_lock);

				for (unsigned i = 0; i < MAX_NODE_HANDLES; i++)
					if (!_nodes[i]) {
						_nodes[i] = node;
						return i;
					}

				throw Out_of_node_handles();
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
				Lock::Guard guard(_lock);

				if (_in_range(handle.value))
					_nodes[handle.value] = 0;
			}

			/**
			 * Lookup node using its handle as key
			 *
			 * \throw Invalid_handle
			 */
			template <typename HANDLE_TYPE>
			typename Node_type<HANDLE_TYPE>::Type *lookup(HANDLE_TYPE handle)
			{
				Lock::Guard guard(_lock);

				if (!_in_range(handle.value))
					throw Invalid_handle();

				typedef typename Node_type<HANDLE_TYPE>::Type Node;
				Node *node = dynamic_cast<Node *>(_nodes[handle.value]);
				if (!node)
					throw Invalid_handle();

				return node;
			}

			bool refer_to_same_node(Node_handle h1, Node_handle h2) const
			{
				Lock::Guard guard(_lock);

				if (!_in_range(h1.value) || !_in_range(h2.value))
					throw Invalid_handle();

				return _nodes[h1.value] == _nodes[h2.value];
			}
	};
}

#endif /* _NODE_HANDLE_REGISTRY_H_ */
