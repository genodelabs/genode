/*
 * \brief  Heap that stores each block at a separate dataspace
 * \author Norman Feske
 * \date   2006-08-16
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/heap.h>
#include <base/printf.h>

namespace Genode {

	class Sliced_heap::Block : public List<Sliced_heap::Block>::Element
	{
		private:

			Ram_dataspace_capability _ds_cap;
			size_t                   _size;
			char                     _data[];

		public:

			inline void *operator new(size_t size, void *at_addr) {
				return at_addr; }

			inline void operator delete (void*) { }

			/**
			 * Constructor
			 */
			Block(Ram_dataspace_capability ds_cap, size_t size):
				_ds_cap(ds_cap), _size(size) { }

			/**
			 * Accessors
			 */
			Ram_dataspace_capability ds_cap()     { return  _ds_cap; }
			size_t                   size()       { return  _size; }
			void                    *data_start() { return &_data[0]; }

			/**
			 * Lookup Slab_entry by given address
			 *
			 * The specified address is supposed to point to data[0].
			 */
			static Block *block(void *addr) {
				return (Block *)((addr_t)addr - sizeof(Block)); }
	};
}

using namespace Genode;


Sliced_heap::Sliced_heap(Ram_session *ram_session, Rm_session *rm_session):
	_ram_session(ram_session), _rm_session(rm_session),
	_consumed(0) { }


Sliced_heap::~Sliced_heap()
{
	for (Block *b; (b = _block_list.first()); )
		free(b->data_start(), b->size()); }


bool Sliced_heap::alloc(size_t size, void **out_addr)
{
	/* serialize access to block list */
	Lock::Guard lock_guard(_lock);

	/* allocation includes space for block meta data and is page-aligned */
	size = align_addr(size + sizeof(Block), 12);

	Ram_dataspace_capability ds_cap;
	void *local_addr;

	try {
		ds_cap     = _ram_session->alloc(size);
		local_addr = _rm_session->attach(ds_cap);
	} catch (Rm_session::Attach_failed) {
		PERR("Could not attach dataspace to local address space");
		_ram_session->free(ds_cap);
		return false;
	} catch (Ram_session::Alloc_failed) {
		PERR("Could not allocate dataspace with size %zd", size);
		return false;
	}

	Block *b = new(local_addr) Block(ds_cap, size);
	_consumed += size;
	_block_list.insert(b);
	*out_addr = b->data_start();
	return true;
}


void Sliced_heap::free(void *addr, size_t size)
{
	/* serialize access to block list */
	Lock::Guard lock_guard(_lock);

	Block *b = Block::block(addr);
	_block_list.remove(b);
	_consumed -= b->size();
	Ram_dataspace_capability ds_cap = b->ds_cap();
	delete b;
	_rm_session->detach(b);
	_ram_session->free(ds_cap);
}


size_t Sliced_heap::overhead(size_t size) { return align_addr(size + sizeof(Block), 12) - size; }
