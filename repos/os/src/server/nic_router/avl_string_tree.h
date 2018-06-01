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

		OBJECT &_find_by_name(char const *name)
		{
			if (!first()) {
				throw No_match(); }

			Node *node = first()->find_by_name(name);
			if (!node) {
				throw No_match(); }

			return *static_cast<OBJECT *>(node);
		}

	public:

		struct No_match        : Genode::Exception { };
		struct Name_not_unique : Genode::Exception
		{
			OBJECT &object;

			Name_not_unique(OBJECT &object) : object(object) { }
		};

		OBJECT &find_by_name(NAME const &name) { return _find_by_name(name.string()); }

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

		void insert(OBJECT &object)
		{
			try { throw Name_not_unique(_find_by_name(static_cast<Node *>(&object)->name())); }
			catch (No_match) { Tree::insert(&object); }
		}

		void remove(OBJECT &object) { Tree::remove(&object); }
};



#endif /* _AVL_STRING_TREE_H_ */
