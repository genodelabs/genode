/*
 * \brief  Utility for finding objecs by name
 * \author Norman Feske
 * \date   2021-11-11
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NAMED_REGISTRY_H_
#define _NAMED_REGISTRY_H_

#include <util/string.h>
#include <util/avl_tree.h>
#include <util/noncopyable.h>

namespace Driver { template <typename T> class Named_registry; }


template <typename T>
class Driver::Named_registry : Noncopyable
{
	private:

		Avl_tree<T> _tree { };

	public:

		class Element : private Avl_node<T>
		{
			public:

				using Name = Genode::String<64>;
				Name const name;

			private:

				Named_registry<T> &_registry;

				bool higher(T const *other) const { return name > other->name; }

				friend class Avl_tree<T>;
				friend class Avl_node<T>;
				friend class Named_registry<T>;

				static T *_matching_sub_tree(T &curr, Name const &name)
				{
					typename Avl_node<T>::Side side = (curr.name > name);

					return curr.Avl_node<T>::child(side);
				}

			public:

				Element(Named_registry &registry, Name const &name)
				:
					name(name), _registry(registry)
				{
					_registry._tree.insert(this);
				}

				~Element()
				{
					_registry._tree.remove(this);
				}
		};

		template <typename FN>
		void apply(typename Element::Name const &name, FN const &fn)
		{
			T *curr_ptr = _tree.first();
			for (;;) {
				if (!curr_ptr)
					return;

				if (curr_ptr->name == name) {
					fn(*curr_ptr);
					return;
				}

				curr_ptr = Element::_matching_sub_tree(*curr_ptr, name);
			}
		}
};

#endif /* _NAMED_REGISTRY_H_ */
