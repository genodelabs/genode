/*
 * \brief  Region map for l4lx support library.
 * \author Stefan Kalkowski
 * \date   2011-04-11
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _L4LX__RM_H_
#define _L4LX__RM_H_

/* Genode includes */
#include <base/allocator_avl.h>
#include <util/avl_tree.h>
#include <util/list.h>

/* L4lx includes */
#include <dataspace.h>

namespace L4lx {

	class Region
	{
		private:

			Genode::addr_t  _addr;
			Genode::size_t  _size;
			Dataspace      *_ds;

		public:


			/******************
			 ** Constructors **
			 ******************/

			Region(Genode::addr_t addr, Genode::size_t size)
			: _addr(addr), _size(size), _ds(0) {}

			Region(Genode::addr_t addr, Genode::size_t size, Dataspace *ds)
			: _addr(addr), _size(size), _ds(ds) {}


			/***************
			 ** Accessors **
			 ***************/

			Genode::addr_t  addr() { return _addr; }
			Genode::size_t  size() { return _size; }
			Dataspace      *ds()   { return _ds;   }
	};


	class Mapping : public Genode::Avl_node<Mapping>,
	                public Genode::List<Mapping>::Element
	{
		private:

			void *_virt;
			void *_phys;
			bool  _rw;

		public:

			Mapping(void *virt, void *v, bool rw)
			: _virt(virt), _phys(v), _rw(rw) { }

			/************************
			 ** Avl node interface **
			 ************************/

			void *phys()     { return _phys; }
			void *virt()       { return _virt;   }
			bool  writeable() { return _rw;    }

			bool higher(Mapping *m) { return m->_virt > _virt; }

			Mapping *find_by_virt(void *virt)
			{
				if (virt == _virt) return this;

				Mapping *m = Genode::Avl_node<Mapping>::child(virt > _virt);
				return m ? m->find_by_virt(virt) : 0;
			}
	};


	class Phys_mapping : public Genode::Avl_node<Phys_mapping>
	{
		private:

			void                 *_phys;
			Genode::List<Mapping> _list;

		public:

			Phys_mapping(void *phys) : _phys(phys) { }

			Genode::List<Mapping> *mappings() { return &_list; }

			bool higher(Phys_mapping *m) { return m->_phys > _phys; }

			Phys_mapping *find_by_phys(void *phys)
			{
				if (phys == _phys) return this;

				Phys_mapping *m = Genode::Avl_node<Phys_mapping>::child(phys > _phys);
				return m ? m->find_by_phys(phys) : 0;
			}
	};


	class Region_manager : public Genode::Allocator_avl_tpl<Region>
	{
		private:

			Genode::Avl_tree<Mapping>      _virt_tree;
			Genode::Avl_tree<Phys_mapping> _phys_tree;

			Mapping*      _virt_to_phys(void *virt);
			Phys_mapping* _phys_to_virt(void *phys);

		public:

			/*****************
			 ** Constructor **
			 *****************/

			Region_manager(Allocator *md_alloc)
			: Genode::Allocator_avl_tpl<Region>(md_alloc) { }


			/***************************
			 ** Convenience functions **
			 ***************************/

			Region *find_region(Genode::addr_t *addr, Genode::size_t *size);
			void   *attach(Genode::Dataspace_capability cap, const char* name);
			void   *attach(Dataspace *ds);
			bool    attach_at(Dataspace *ds, Genode::size_t size,
			                  Genode::size_t offset, void* addr);
			Region *reserve_range(Genode::size_t size, int align,
			                      Genode::addr_t start=0);
			void    reserve_range(Genode::addr_t addr, Genode::size_t size,
			                      const char *name);

			void  add_mapping(void *phys, void *virt, bool rw);
			void  remove_mapping(void *virt);
			void  map(void *phys);
			void* phys(void *virt);

			/***********
			 ** Debug **
			 ***********/

			void  dump();
	};
}

#endif /* _L4LX__RM_H_ */
