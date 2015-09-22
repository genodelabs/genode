/*
 * \brief  Slightly improved list
 * \author Christian Helmuth
 * \date   2014-09-25
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LIST_H_
#define _LIST_H_

#include <util/list.h>


namespace Lx {
	template <typename> class List;
	template <typename> class List_element;
}

template <typename LT>
class Lx::List : private Genode::List<LT>
{
	private:

		typedef Genode::List<LT> Base;

	public:

		using Base::Element;

		void append(LT const *le)
		{
			LT *at = nullptr;

			for (LT *l = first(); l; l = l->next())
				at = l;

			Base::insert(le, at);
		}

		void prepend(LT const *le)
		{
			Base::insert(le);
		}

		void insert_before(LT const *le, LT const *at)
		{
			if (at == first()) {
				prepend(le);
				return;
			} else if (!at) {
				append(le);
				return;
			}

			for (LT *l = first(); l; l = l->next())
				if (l->next() == at)
					at = l;

			Base::insert(le, at);
		}


		/****************************
		 ** Genode::List interface **
		 ****************************/

		LT       *first()       { return Base::first(); }
		LT const *first() const { return Base::first(); }

		void insert(LT const *le, LT const *at = 0)
		{
			Base::insert(le, at);
		}

		void remove(LT const *le)
		{
			Base::remove(le);
		}
};


template <typename T>
class Lx::List_element : public Lx::List<List_element<T> >::Element
{
	private:

		T *_object;

	public:

		List_element(T *object) : _object(object) { }

		T *object() const { return _object; }
};

#endif /* _LIST_H_ */
