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
			virtual bool map(Genode::size_t offset)    = 0;

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

			Genode::Dataspace_capability cap() { return _cap; }
			bool map(Genode::size_t offset)    { return true; }
	};


	class Chunked_dataspace : public Dataspace
	{
		private:

			class Chunk : public Genode::Avl_node<Chunk>
			{
				private:

					Genode::size_t               _offset;
					Genode::size_t               _size;
					Genode::Dataspace_capability _cap;

				public:

					Chunk(Genode::size_t off, Genode::size_t size,
					      Genode::Dataspace_capability cap)
					: _offset(off), _size(size), _cap(cap) {}

					Genode::size_t               offset() { return _offset; }
					Genode::size_t               size()   { return _size;   }
					Genode::Dataspace_capability cap()    { return _cap;    }

					bool higher(Chunk *n) { return n->_offset > _offset; }

					Chunk *find_by_offset(Genode::size_t off)
					{
						if (off >= _offset && off < _offset+_size) return this;

						Chunk *n = Genode::Avl_node<Chunk>::child(off > _offset);
						return n ? n->find_by_offset(off) : 0;
					}
			};

			Genode::Rm_connection   _rm;
			Genode::Avl_tree<Chunk> _chunks;
			Genode::size_t          _chunk_size;
			Genode::size_t          _chunk_size_log2;

	public:

			Chunked_dataspace(const char*          name,
							  Genode::size_t       size,
							  Fiasco::l4_cap_idx_t ref,
			                  Genode::size_t       chunk_size)
			: Dataspace(name, size, ref), _rm(0, size), _chunk_size(chunk_size),
			  _chunk_size_log2(Genode::log2(_chunk_size)) {}

			Genode::Dataspace_capability cap() { return _rm.dataspace(); }

			bool map(Genode::size_t off)
			{
				off = Genode::align_addr((off-(_chunk_size-1)), _chunk_size_log2);

				Chunk* c = _chunks.first() ? _chunks.first()->find_by_offset(off) : 0;
				if (c) return true;

				try {
					Genode::Dataspace_capability cap =
						Genode::env()->ram_session()->alloc(_chunk_size);
					_chunks.insert(new (Genode::env()->heap())
								   Chunk(off, _chunk_size, cap));
					_rm.attach(cap, 0, 0, true, off);
					return true;
				} catch(Genode::Ram_session::Quota_exceeded) {
					PWRN("Could not allocate new dataspace chunk");
				} catch(Genode::Rm_session::Attach_failed) {
					PWRN("Attach of chunk dataspace of size %zx to %p failed",
					     _chunk_size, (void*) off);
				}
				return false;
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
