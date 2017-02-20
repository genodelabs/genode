/*
 * \brief  Queue for delayed object destruction
 * \author Christian Prochaska
 * \date   2013-01-03
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOUX__DESTRUCT_QUEUE_H_
#define _NOUX__DESTRUCT_QUEUE_H_

/* Genode includes */
#include <base/allocator.h>
#include <util/list.h>

namespace Noux { class Destruct_queue; }


class Noux::Destruct_queue
{
	public:

		struct Element_base : Genode::List<Element_base>::Element
		{
			virtual void destroy() = 0;
		};

		/*
		 * When a pointer to an object which inherits 'Element' among other
		 * base classes gets static-casted to a pointer to the 'Element'
		 * base object, the resulting address can differ from the start
		 * address of the inherited object. To be able to pass the start
		 * address of the inherited object to the allocator, a static-cast
		 * back to the inherited class needs to be performed. Therefore the
		 * type of the class inheriting from 'Element' needs to be given as
		 * template parameter.
		 */
		template <typename T>
		class Element : public Element_base
		{
			private:

				Genode::Allocator &_alloc;

			public:

				/**
				 * Constructor
				 *
				 * \param alloc  the allocator which was used to allocate
				 *               the element
				 */
				Element(Genode::Allocator &alloc) : _alloc(alloc) { }

				virtual ~Element() { };

				void destroy()
				{
					Genode::destroy(_alloc, static_cast<T*>(this));
				}
		};

	private:

		Genode::List<Element_base> _destruct_list;
		Genode::Lock               _destruct_list_lock;
		Signal_context_capability  _sigh;

	public:

		Destruct_queue(Signal_context_capability sigh) : _sigh(sigh) { }

		void insert(Element_base *element)
		{
			Genode::Lock::Guard guard(_destruct_list_lock);
			_destruct_list.insert(element);

			Signal_transmitter(_sigh).submit();
		}

		void flush()
		{
			Genode::Lock::Guard guard(_destruct_list_lock);

			Element_base *element;
			while ((element = _destruct_list.first())) {
				_destruct_list.remove(element);
				element->destroy();
			}
		}
};

#endif /* _NOUX__DESTRUCT_QUEUE_H_ */
