/*
 * \brief  Dataspace abstraction between Genode and L4Linux
 * \author Stefan Kalkowski
 * \date   2011-03-20
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _L4LX__DATASPACE_H_
#define _L4LX__DATASPACE_H_

/* Genode includes */
#include <dataspace/client.h>
#include <base/cap_map.h>
#include <base/env.h>
#include <util/avl_tree.h>
#include <rm_session/connection.h>
#include <platform_env.h>

namespace Fiasco {
#include <l4/sys/types.h>
}

namespace L4lx {

	class Dataspace : public Genode::Avl_node<Dataspace>
	{
		private:

			const char*          _name;
			Genode::size_t       _size;
			Fiasco::l4_cap_idx_t _ref;

		public:

			/******************
			 ** Constructors **
			 ******************/

			Dataspace(const char*          name,
			          Genode::size_t       size,
			          Fiasco::l4_cap_idx_t ref)
			: _name(name), _size(size), _ref(ref) {}


			/***************
			 ** Accessors **
			 ***************/

			const char*                  name() const { return _name; }
			Genode::size_t               size()       { return _size; }
			Fiasco::l4_cap_idx_t         ref()        { return _ref;  }

			virtual Genode::Dataspace_capability cap() = 0;
			virtual void map(Genode::size_t offset, bool greedy = false) = 0;
			virtual bool free(Genode::size_t offset)   = 0;

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


	class Single_dataspace : public Dataspace
	{
		private:

			Genode::Dataspace_capability _cap;

		public:

			Single_dataspace(const char*                    name,
							 Genode::size_t                 size,
							 Genode::Dataspace_capability   ds,
			                 Fiasco::l4_cap_idx_t           ref =
				Genode::cap_idx_alloc()->alloc_range(1)->kcap())
			: Dataspace(name, size, ref), _cap(ds) {}

			Genode::Dataspace_capability cap() { return _cap;  }
			void map(Genode::size_t offset, bool greedy) { }
			bool free(Genode::size_t offset)   { return false; }
	};


	class Chunked_dataspace : public Dataspace
	{
		private:

			Genode::Rm_connection               _rm_con;
			Genode::Expanding_rm_session_client _rm;
			Genode::Ram_dataspace_capability   *_chunks;

	public:

			enum {
				CHUNK_SIZE_LOG2 = 20,
				CHUNK_SIZE      = 1 << CHUNK_SIZE_LOG2,
			};

			Chunked_dataspace(const char*          name,
							  Genode::size_t       size,
							  Fiasco::l4_cap_idx_t ref)
			: Dataspace(name, size, ref), _rm_con(0, size), _rm(_rm_con.cap())
			{
				_chunks = (Genode::Ram_dataspace_capability*) Genode::env()->heap()->alloc(
					sizeof(Genode::Ram_dataspace_capability) * (size/CHUNK_SIZE));
			}

			Genode::Dataspace_capability cap() { return _rm_con.dataspace(); }

			void map(Genode::size_t off, bool greedy)
			{
				off = Genode::align_addr((off-(CHUNK_SIZE-1)), CHUNK_SIZE_LOG2);
				int i = off / CHUNK_SIZE;
				if (_chunks[i].valid()) return;

				Genode::size_t ram_avail = Genode::env()->ram_session()->avail();
				if (greedy && ram_avail < 4*CHUNK_SIZE) {
					char buf[128];
					Genode::snprintf(buf, sizeof(buf), "ram_quota=%zd",
					                 4*CHUNK_SIZE - ram_avail);
					Genode::env()->parent()->resource_request(buf);
				}

				_chunks[i] = Genode::env()->ram_session()->alloc(CHUNK_SIZE);
				_rm.attach(_chunks[i], 0, 0, true, off, false);
			}

			bool free(Genode::size_t off)
			{
				off = Genode::align_addr((off-(CHUNK_SIZE-1)), CHUNK_SIZE_LOG2);
				int i = off / CHUNK_SIZE;
				if (!_chunks[i].valid()) return false;
				Genode::env()->ram_session()->free(_chunks[i]);
				_chunks[i] = Genode::Ram_dataspace_capability();
				return true;
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
