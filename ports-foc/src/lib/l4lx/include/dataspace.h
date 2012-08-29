/*
 * \brief  Dataspace abstraction between Genode and L4Linux
 * \author Stefan Kalkowski
 * \date   2011-03-20
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _L4LX__DATASPACE_H_
#define _L4LX__DATASPACE_H_

/* Genode includes */
#include <dataspace/client.h>
#include <base/cap_map.h>
#include <util/avl_tree.h>

namespace Fiasco {
#include <l4/sys/types.h>
}

namespace L4lx {

	class Dataspace : public Genode::Avl_node<Dataspace>
	{
		private:

			const char*                  _name;
			Genode::size_t               _size;
			Genode::Dataspace_capability _cap;
			Fiasco::l4_cap_idx_t         _ref;

		public:

			/******************
			 ** Constructors **
			 ******************/

			Dataspace(const char*                    name,
			          Genode::size_t                 size,
			          Genode::Dataspace_capability   ds,
			          Fiasco::l4_cap_idx_t           ref)
			: _name(name), _size(size), _cap(ds), _ref(ref) {}

			Dataspace(const char*                    name,
			          Genode::size_t                 size,
			          Genode::Dataspace_capability   ds)
			: _name(name), _size(size), _cap(ds),
				_ref(Genode::cap_idx_alloc()->alloc_range(1)->kcap()) {}


			/***************
			 ** Accessors **
			 ***************/

			const char*                  name() const { return _name; }
			Genode::size_t               size()       { return _size; }
			Genode::Dataspace_capability cap()        { return _cap;  }
			Fiasco::l4_cap_idx_t         ref()        { return _ref;  }


			/************************
			 ** Avl_node interface **
			 ************************/

			bool higher(Dataspace *n) { return n->_ref > _ref; }

			Dataspace *find_by_ref(Fiasco::l4_cap_idx_t ref)
			{
				if (_ref == ref) return this;

				Dataspace *n = Genode::Avl_node<Dataspace>::child(ref > _ref);
				return n ? n->find_by_ref(ref) : 0;
			}
	};


	class Dataspace_tree : public Genode::Avl_tree<Dataspace>
	{
		public:

			Dataspace* find_by_ref(Fiasco::l4_cap_idx_t ref) {
				return this->first() ? this->first()->find_by_ref(ref) : 0; }

			Dataspace* insert(const char* name, Genode::Dataspace_capability cap);

			void insert(Dataspace *ds) { Genode::Avl_tree<Dataspace>::insert(ds); }
	};
}

#endif /* _L4LX__DATASPACE_H_ */
