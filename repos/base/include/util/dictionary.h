/*
 * \brief  Utility for accessing objects by name
 * \author Norman Feske
 * \date   2022-09-14
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__DICTIONARY_H_
#define _INCLUDE__UTIL__DICTIONARY_H_

#include <util/meta.h>
#include <util/string.h>
#include <util/avl_tree.h>
#include <util/noncopyable.h>
#include <base/log.h>

namespace Genode { template <typename, typename> class Dictionary; }


template <typename T, typename NAME>
class Genode::Dictionary : Noncopyable
{
	private:

		Avl_tree<T> _tree { };

	public:

		class Element : private Avl_node<T>
		{
			public:

				NAME const name;

			private:

				using This = Dictionary<T, NAME>::Element;

				Dictionary<T, NAME> &_dictionary;

				bool higher(T const *other) const { return name > other->This::name; }

				friend class Avl_tree<T>;
				friend class Avl_node<T>;
				friend class Dictionary<T, NAME>;

				static T *_matching_sub_tree(T &curr, NAME const &name)
				{
					typename Avl_node<T>::Side side = (curr.This::name > name);
					return curr.Avl_node<T>::child(side);
				}

			public:

				Element(Dictionary &dictionary, NAME const &name)
				:
					name(name), _dictionary(dictionary)
				{
					if (_dictionary.exists(name))
						warning("dictionary entry '", name, "' is not unique");

					_dictionary._tree.insert(this);
				}

				~Element()
				{
					_dictionary._tree.remove(this);
				}
		};

		/**
		 * Call 'match_fn' with named constant dictionary element
		 *
		 * The 'match_fn' functor is called with a const reference to the
		 * matching dictionary element. If no maching element exists,
		 * 'no_match_fn' is called without argument.
		 */
		template <typename FN1, typename FN2>
		auto with_element(NAME const &name, FN1 const &match_fn, FN2 const &no_match_fn)
		-> typename Trait::Functor<decltype(&FN1::operator())>::Return_type
		{
			T *curr_ptr = _tree.first();
			for (;;) {
				if (!curr_ptr)
					break;

				if (curr_ptr->Element::name == name) {
					return match_fn(*curr_ptr);
				}

				curr_ptr = Element::_matching_sub_tree(*curr_ptr, name);
			}
			return no_match_fn();
		}

		/**
		 * Call 'match_fn' with named mutable dictionary element
		 *
		 * The 'match_fn' functor is called with a non-const reference to the
		 * matching dictionary element. If no maching element exists,
		 * 'no_match_fn' is called without argument.
		 */
		template <typename FN1, typename FN2>
		auto with_element(NAME const &name, FN1 const &match_fn, FN2 const &no_match_fn) const
		-> typename Trait::Functor<decltype(&FN1::operator())>::Return_type
		{
			auto const_match_fn = [&] (T const &e) { return match_fn(e); };
			auto non_const_this = const_cast<Dictionary *>(this);
			return non_const_this->with_element(name, const_match_fn, no_match_fn);
		}

		/**
		 * Call 'fn' with a non-const reference to any dictionary element
		 *
		 * \return true  if 'fn' was called, or
		 *         false if the dictionary is empty
		 *
		 * This method is intended for the orderly destruction of a dictionary.
		 * It allows for the consecutive destruction of all elements.
		 */
		template <typename FUNC>
		bool with_any_element(FUNC const &fn)
		{
			T *curr_ptr = _tree.first();
			if (!curr_ptr)
				return false;

			fn(*curr_ptr);
			return true;
		}

		template <typename FN>
		void for_each(FN const &fn) const { _tree.for_each(fn); }

		bool exists(NAME const &name) const
		{
			return with_element(name, [] (T const &) { return true;  },
			                          []             { return false; });
		}
};

#endif /* _INCLUDE__UTIL__DICTIONARY_H_ */
