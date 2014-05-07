/*
 * \brief   Memory subsystem
 * \author  Christian Helmuth
 * \date    2008-08-15
 *
 * Open issues:
 *
 * - Rethink file split up
 * - What about slabs for large malloc blocks?
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/lock.h>
#include <base/printf.h>
#include <base/slab.h>
#include <util/avl_tree.h>
#include <util/misc_math.h>
#include <dataspace/client.h>

#include <os/attached_ram_dataspace.h>

extern "C" {
#include <dde_kit/memory.h>
#include <dde_kit/pgtab.h>
}

using namespace Genode;


/*****************************
 ** Backing store allocator **
 *****************************/

/*
 * The backing store allocator allocates RAM dataspaces and maintains virtual
 * page-table entries.
 */
class Backing_store_allocator : public Allocator
{
	private:

		class Block : public Avl_node<Block>
		{
			private:

				Attached_ram_dataspace *_ram_ds;

			public:

				Block(Attached_ram_dataspace *ram_ds) : _ram_ds(ram_ds) { }

				Attached_ram_dataspace *ram_ds() const { return _ram_ds; }

				/* AVL node comparison */
				bool higher(Block *b) {
					return _ram_ds->local_addr<void>() < b->_ram_ds->local_addr<void>(); }

				/* AVL node lookup */
				Block *lookup(void *virt)
				{
					if (virt == _ram_ds->local_addr<void>()) return this;

					Block *b = child(_ram_ds->local_addr<void>() < virt);
					return b ? b->lookup(virt) : 0;
				}
		};

		Avl_tree<Block> _map;       /* backing store block map */
		size_t          _consumed;  /* allocated memory size */

		Lock            _lock;      /* synchronize access to allocator */

	public:

		Backing_store_allocator() { }
		virtual ~Backing_store_allocator() { }

		/***************
		 ** Allocator **
		 ***************/

		bool alloc(size_t size, void **out_addr)
		{
			Lock::Guard lock_guard(_lock);

			Attached_ram_dataspace *ram_ds;

			/* alloc and attach RAM dataspace */
			try {
				ram_ds = new (env()->heap()) Attached_ram_dataspace(env()->ram_session(), size);
			} catch (...) {
				PERR("RAM allocation failed (size=%zx)", size);
				return false;
			}

			/* setup virt->phys and phys->virt mappings */
			void  *virt = ram_ds->local_addr<void>();
			addr_t phys = Dataspace_client(ram_ds->cap()).phys_addr();

			dde_kit_pgtab_set_region_with_size(virt, phys, size);

			/* store allocation in our map */
			Block *b = new (env()->heap()) Block(ram_ds);

			/* cleanup on error */
			if (!b) {
				dde_kit_pgtab_clear_region(virt);
				destroy(env()->heap(), ram_ds);
				return false;
			}

			_map.insert(b);
			_consumed += size;

			*out_addr = virt;
			return true;
		}

		void free(void *virt, size_t size)
		{
			Lock::Guard lock_guard(_lock);

			/* lookup address */
			Block *b = _map.first() ? _map.first()->lookup(virt) : 0;
			if (!b) return;

			Attached_ram_dataspace *ram_ds = b->ram_ds();

			/*
			 * We support freeing of the whole block only.
			 */
			if (size && (size != dde_kit_pgtab_get_size(virt)))
				PERR("Cannot split RAM allocations - the whole block is free'd.");

			/* remove allocation from our map */
			_map.remove(b);

			/* delete virt->phys and phys->virt mappings */
			dde_kit_pgtab_clear_region(virt);

			/* detach and free RAM dataspace */
			destroy(env()->heap(), ram_ds);

			/* free meta data */
			destroy(env()->heap(), b);
		}

		size_t consumed() { return _consumed; }

		size_t overhead(size_t size) { return 0; }

		bool need_size_for_free() const override { return false; }
};


/**
 * Get backing store allocator
 *
 * \return  reference to static backing store object
 */
static Backing_store_allocator * backing_store_allocator()
{
	static Backing_store_allocator _backing_store_allocator;

	return &_backing_store_allocator;
}


/*******************
 ** Slab facility **
 *******************/

struct dde_kit_slab : public Slab
{
	private:

		void   *_data;
		size_t  _object_size;

		/*
		 * Each slab in the slab cache contains about 8 objects as proposed in
		 * the paper by Bonwick and block sizes are multiples of page size.
		 */
		size_t _calculate_block_size(size_t object_size)
		{
			size_t block_size = 8 * (object_size + sizeof(Slab_entry)) + sizeof(Slab_block);

			return align_addr(block_size, DDE_KIT_PAGE_SHIFT);
		}

	public:

		dde_kit_slab(size_t object_size)
		:
			Slab(object_size, _calculate_block_size(object_size), 0, backing_store_allocator()),
			_object_size(object_size) { }

		inline void *alloc()
		{
			void *result;
			return (Slab::alloc(_object_size, &result) ? result : 0);
		}

		inline void *data() const    { return _data; }
		inline void data(void *data) { _data = data; }
};


extern "C" void dde_kit_slab_set_data(struct dde_kit_slab * slab, void *data) {
	slab->data(data); }


extern "C" void *dde_kit_slab_get_data(struct dde_kit_slab * slab) {
	return slab->data(); }



extern "C" void *dde_kit_slab_alloc(struct dde_kit_slab * slab) {
	return slab->alloc(); }


extern "C" void dde_kit_slab_free(struct dde_kit_slab * slab, void *objp) {
	slab->free(objp); }


extern "C" void dde_kit_slab_destroy(struct dde_kit_slab * slab) {
	destroy(env()->heap(), slab); }


extern "C" struct dde_kit_slab * dde_kit_slab_init(unsigned size)
{
	dde_kit_slab *slab_cache;

	try {
		slab_cache = new (env()->heap()) dde_kit_slab(size);
	} catch (...) {
		PERR("allocation failed (size=%zd)", sizeof(*slab_cache));
		return 0;
	}

	return slab_cache;
}


/**********************************
 ** Large-block memory allocator **
 **********************************/

extern "C" void *dde_kit_large_malloc(dde_kit_size_t size)
{
	void *virt;

	if (!backing_store_allocator()->alloc(size, &virt))
		return 0;

	return virt;
}


extern "C" void  dde_kit_large_free(void *virt) {
	backing_store_allocator()->free(virt, 0);
}


/*****************************
 ** Simple memory allocator **
 *****************************/

extern "C" void *dde_kit_simple_malloc(dde_kit_size_t size)
{
	/*
	 * We store the size of the allocation at the very
	 * beginning of the allocated block and return
	 * the subsequent address. This way, we can retrieve
	 * the size information when freeing the block.
	 */
	size_t  real_size = size + sizeof(size_t);
	size_t *addr;
	try {
		addr = (size_t *)env()->heap()->alloc(real_size);
	} catch (...) {
		return 0;
	}

	*addr = real_size;
	return addr + 1;
}


extern "C" void dde_kit_simple_free(void *p)
{
	if (!p) return;

	size_t *addr = ((size_t *)p) - 1;
	env()->heap()->free(addr, *addr);
}
