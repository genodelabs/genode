/*
 * \brief  AVL tree of strings with additional functions needed by NIC router
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _AVL_STRING_TREE_H_
#define _AVL_STRING_TREE_H_

/* local includes */
#include <reference.h>

/* Genode includes */
#include <util/avl_string.h>
#include <base/allocator.h>

namespace Net { template <typename, typename> class Avl_string_tree; }


template <typename OBJECT,
          typename NAME>
class Net::Avl_string_tree : public Genode::Avl_tree<Genode::Avl_string_base>
{
	private:

		using Node = Genode::Avl_string_base;
		using Tree = Genode::Avl_tree<Genode::Avl_string_base>;

		template <typename HANDLE_MATCH_FN,
		          typename HANDLE_NO_MATCH_FN>

		void _node_find_by_name(Node                  &node,
		                        char            const *name_ptr,
		                        HANDLE_MATCH_FN    &&  handle_match,
		                        HANDLE_NO_MATCH_FN &&  handle_no_match)
		{
			int const name_diff {
				Genode::strcmp(name_ptr, node.name()) };

			if (name_diff != 0) {

				Node *child_ptr { node.child(name_diff > 0) };

				if (child_ptr != nullptr) {

					_node_find_by_name(
						*child_ptr, name_ptr, handle_match, handle_no_match);

				} else {

					handle_no_match();
				}
			} else {

				handle_match(*static_cast<OBJECT *>(&node));
			}
		}

		template <typename HANDLE_MATCH_FN,
		          typename HANDLE_NO_MATCH_FN>

		void _find_by_name(char            const *name_ptr,
		                   HANDLE_MATCH_FN    &&  handle_match,
		                   HANDLE_NO_MATCH_FN &&  handle_no_match)
		{
			if (first() != nullptr) {

				_node_find_by_name(
					*first(), name_ptr, handle_match, handle_no_match);

			} else {

				handle_no_match();
			}
		}

	public:

		template <typename HANDLE_MATCH_FN,
		          typename HANDLE_NO_MATCH_FN>

		void find_by_name(NAME            const &name,
		                  HANDLE_MATCH_FN    &&  handle_match,
		                  HANDLE_NO_MATCH_FN &&  handle_no_match)
		{
			_find_by_name(name.string(), handle_match, handle_no_match);
		}

		template <typename FUNCTOR>
		void for_each(FUNCTOR && functor) const {
			Tree::for_each([&] (Node const &node) {

				/*
				 * FIXME This constness cast sneaked in with an older
				 *       implementation where it was done implicitely and
				 *       therefore not that obvious. Now the router relies on
				 *       it and we should either get rid of the dependency to
				 *       const Avl_tree::for_each or of the the need for
				 *       mutable objects in Avl_string_tree::for_each.
				 */
				functor(*const_cast<OBJECT *>(static_cast<OBJECT const *>(&node)));
			});
		}

		void destroy_each(Genode::Deallocator &dealloc)
		{
			while (Node *node = first()) {
				Tree::remove(node);
				Genode::destroy(dealloc, static_cast<OBJECT *>(node));
			}
		}

		template <typename HANDLE_NAME_NOT_UNIQUE_FN>

		void insert(OBJECT                       &obj,
		            HANDLE_NAME_NOT_UNIQUE_FN &&  handle_name_not_unique)
		{
			_find_by_name(
				static_cast<Node *>(&obj)->name(),
				[&] /* handle_match */ (OBJECT &other_obj)
				{
					handle_name_not_unique(other_obj);
				},
				[&] /* handle_no_match */ () { Tree::insert(&obj); }
			);
		}

		void remove(OBJECT &object) { Tree::remove(&object); }
};



#endif /* _AVL_STRING_TREE_H_ */
