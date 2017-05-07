/*
 * \brief  Slab allocator
 * \author Norman Feske
 * \date   2006-04-18
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__SLAB_H_
#define _INCLUDE__BASE__SLAB_H_

#include <base/allocator.h>
#include <base/stdint.h>

namespace Genode { class Slab; }

/**
 * Transitional type definition, for API compatibility only
 *
 * \deprecated  To be removed once all Slab users are updated.
 */
namespace Genode { typedef void Slab_block; }


/**
 * Slab allocator
 */
class Genode::Slab : public Allocator
{
	private:

		struct Block;
		struct Entry;

		size_t const _slab_size;          /* size of one slab entry           */
		size_t const _block_size;         /* size of slab block               */
		size_t const _entries_per_block;  /* number of slab entries per block */

		Block       *_initial_sb;    /* initial (static) slab block        */
		bool         _nested;        /* indicator for nested call of alloc */

		size_t _num_blocks  = 0;
		size_t _total_avail = 0;

		/**
		 * Block used for attempting the next allocation
		 */
		Block *_curr_sb = nullptr;

		Allocator   *_backing_store;

		/**
		 * Allocate and initialize new slab block
		 */
		Block *_new_slab_block();


		/*****************************
		 ** Methods used by 'Block' **
		 *****************************/

		void _release_backing_store(Block *);

		/**
		 * Insert block into slab block ring
		 *
		 * \noapi
		 */
		void _insert_sb(Block *);

		/**
		 * Release slab block
		 */
		void _free_curr_sb();

		/**
		 * Free slab entry
		 */
		void _free(void *addr);

	public:

		/**
		 * Constructor
		 *
		 * At construction time, there exists one initial slab
		 * block that is used for the first couple of allocations,
		 * especially for the allocation of the second slab
		 * block.
		 */
		Slab(size_t slab_size, size_t block_size, void *initial_sb,
		     Allocator *backing_store = 0);

		/**
		 * Destructor
		 */
		~Slab();

		/**
		 * Return number of bytes consumed per slab entry
		 *
		 * The function takes the slab-internal meta-data needs and the actual
		 * slab entry into account.
		 */
		static size_t entry_costs(size_t slab_size, size_t block_size);

		/**
		 * Return number of unused slab entries
		 */
		size_t avail_entries() const { return _total_avail; }

		/**
		 * Add new slab block as backing store
		 *
		 * The specified 'ptr' has to point to a buffer with the size of one
		 * slab block.
		 */
		void insert_sb(void *ptr);

		/**
		 * Return a used slab element, or nullptr if empty
		 */
		void *any_used_elem();

		/**
		 * Define/request backing-store allocator
		 *
		 * \noapi
		 */
		void backing_store(Allocator *bs) { _backing_store = bs; }

		/**
		 * Request backing-store allocator
		 *
		 * \noapi
		 */
		Allocator *backing_store() { return _backing_store; }


		/*************************
		 ** Allocator interface **
		 *************************/

		/**
		 * Allocate slab entry
		 *
		 * The 'size' parameter is ignored as only slab entries with
		 * preconfigured slab-entry size are allocated.
		 */
		bool   alloc(size_t size, void **addr) override;
		void   free(void *addr, size_t) override { _free(addr); }
		size_t consumed() const override;
		size_t overhead(size_t) const override { return _block_size/_entries_per_block; }
		bool   need_size_for_free() const override { return false; }
};

#endif /* _INCLUDE__BASE__SLAB_H_ */
