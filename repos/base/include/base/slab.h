/*
 * \brief  Slab allocator
 * \author Norman Feske
 * \date   2006-04-18
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__SLAB_H_
#define _INCLUDE__BASE__SLAB_H_

#include <base/allocator.h>
#include <base/stdint.h>

namespace Genode {

	class Slab;
	class Slab_block;
	class Slab_entry;
}


/**
 * A slab block holds an array of slab entries.
 */
class Genode::Slab_block
{
	public:

		Slab_block *next;  /* next block     */
		Slab_block *prev;  /* previous block */

	private:

		enum { FREE, USED };

		Slab    *_slab;    /* back reference to slab allocator */
		size_t   _avail;   /* free entries of this block       */

		/*
		 * Each slab block consists of three areas, a fixed-size header
		 * that contains the member variables declared above, a byte array
		 * called state table that holds the allocation state for each slab
		 * entry, and an area holding the actual slab entries. The number
		 * of state-table elements corresponds to the maximum number of slab
		 * entries per slab block (the '_num_elem' member variable of the
		 * Slab allocator).
		 */

		char _data[];  /* dynamic data (state table and slab entries) */

		/*
		 * Caution! no member variables allowed below this line!
		 */

		/**
		 * Return the allocation state of a slab entry
		 */
		inline bool state(int idx) { return _data[idx]; }

		/**
		 * Set the allocation state of a slab entry
		 */
		inline void state(int idx, bool state) { _data[idx] = state; }

		/**
		 * Request address of slab entry by its index
		 */
		Slab_entry *slab_entry(int idx);

		/**
		 * Determine block index of specified slab entry
		 */
		int slab_entry_idx(Slab_entry *e);

	public:

		/**
		 * Constructor
		 *
		 * Normally, Slab_blocks are constructed by a Slab allocator
		 * that specifies itself as constructor argument.
		 */
		explicit Slab_block(Slab *s = 0) { if (s) slab(s); }

		/**
		 * Configure block to be managed by the specified slab allocator
		 */
		void slab(Slab *slab);

		/**
		 * Request number of available entries in block
		 */
		unsigned avail() const { return _avail; }

		/**
		 * Allocate slab entry from block
		 */
		void *alloc();

		/**
		 * Return a used slab block entry
		 */
		Slab_entry *first_used_entry();

		/**
		 * These functions are called by Slab_entry.
		 */
		void inc_avail(Slab_entry *e);
		void dec_avail();

		/**
		 * Debug and test hooks
		 */
		void dump();
		int check_bounds();
};


class Genode::Slab_entry
{
	private:

		Slab_block *_sb;
		char        _data[];

		/*
		 * Caution! no member variables allowed below this line!
		 */

	public:

		void init() { _sb = 0; }

		void occupy(Slab_block *sb)
		{
			_sb = sb;
			_sb->dec_avail();
		}

		void free()
		{
			_sb->inc_avail(this);
			_sb = 0;
		}

		void *addr() { return _data; }

		/**
		 * Lookup Slab_entry by given address
		 *
		 * The specified address is supposed to point to _data[0].
		 */
		static Slab_entry *slab_entry(void *addr) {
			return (Slab_entry *)((addr_t)addr - sizeof(Slab_entry)); }
};


/**
 * Slab allocator
 */
class Genode::Slab : public Allocator
{
	private:

		size_t      _slab_size;     /* size of one slab entry               */
		size_t      _block_size;    /* size of slab block                   */
		size_t      _num_elem;      /* number of slab entries per block     */
		Slab_block *_first_sb;      /* first slab block                     */
		Slab_block *_initial_sb;    /* initial (static) slab block          */
		bool        _alloc_state;   /* indicator for 'currently in service' */

		Allocator *_backing_store;

		/**
		 * Allocate and initialize new slab block
		 */
		Slab_block *_new_slab_block();

	public:

		inline size_t slab_size()  const { return _slab_size;  }
		inline size_t block_size() const { return _block_size; }
		inline size_t num_elem()   const { return _num_elem;   }
		inline size_t entry_size() const { return sizeof(Slab_entry) + _slab_size; }

		/**
		 * Constructor
		 *
		 * At construction time, there exists one initial slab
		 * block that is used for the first couple of allocations,
		 * especially for the allocation of the second slab
		 * block.
		 */
		Slab(size_t slab_size, size_t block_size, Slab_block *initial_sb,
		     Allocator *backing_store = 0);

		/**
		 * Destructor
		 */
		~Slab();

		/**
		 * Debug function for dumping the current slab block list
		 *
		 * \noapi
		 */
		void dump_sb_list();

		/**
		 * Remove block from slab block list
		 *
		 * \noapi
		 */
		void remove_sb(Slab_block *sb);

		/**
		 * Insert block into slab block list
		 *
		 * \noapi
		 */
		void insert_sb(Slab_block *sb, Slab_block *at = 0);

		/**
		 * Free slab entry
		 */
		static void free(void *addr);

		/**
		 * Return a used slab element
		 */
		void *first_used_elem();

		/**
		 * Return true if number of free slab entries is higher than n
		 */
		bool num_free_entries_higher_than(int n);

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
		void   free(void *addr, size_t) override { free(addr); }
		size_t consumed() const override;
		size_t overhead(size_t) const override { return _block_size/_num_elem; }
		bool   need_size_for_free() const override { return false; }
};

#endif /* _INCLUDE__BASE__SLAB_H_ */
