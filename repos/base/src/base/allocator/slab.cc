/*
 * \brief  Slab allocator implementation
 * \author Norman Feske
 * \date   2006-05-16
 */

/*
 * Copyright (C) 2006-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/slab.h>
#include <util/construct_at.h>
#include <util/misc_math.h>

using namespace Genode;


/**
 * A slab block holds an array of slab entries.
 */
class Genode::Slab::Block
{
	public:

		Block *next = nullptr;  /* next block     */
		Block *prev = nullptr;  /* previous block */

	private:

		enum { FREE, USED };

		Slab  &_slab;                              /* back reference to slab     */
		size_t _avail = _slab._entries_per_block;  /* free entries of this block */

		/*
		 * Each slab block consists of three areas, a fixed-size header
		 * that contains the member variables declared above, a byte array
		 * called state table that holds the allocation state for each slab
		 * entry, and an area holding the actual slab entries. The number
		 * of state-table elements corresponds to the maximum number of slab
		 * entries per slab block (the '_entries_per_block' member variable of
		 * the Slab allocator).
		 */

		char _data[];  /* dynamic data (state table and slab entries) */

		/*
		 * Caution! no member variables allowed below this line!
		 */

		/**
		 * Return the allocation state of a slab entry
		 */
		inline bool _state(int idx) { return _data[idx]; }

		/**
		 * Set the allocation state of a slab entry
		 */
		inline void _state(int idx, bool state) { _data[idx] = state; }

		/**
		 * Request address of slab entry by its index
		 */
		Entry *_slab_entry(int idx);

		/**
		 * Determine block index of specified slab entry
		 */
		int _slab_entry_idx(Entry *e);

	public:

		/**
		 * Constructor
		 */
		explicit Block(Slab &slab) : _slab(slab)
		{
			for (unsigned i = 0; i < _avail; i++)
				_state(i, FREE);
		}

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
		Entry *any_used_entry();

		/**
		 * These functions are called by Slab::Entry.
		 */
		void inc_avail(Entry &e);
		void dec_avail();

		/**
		 * Debug and test hooks
		 */
		void dump();
		int check_bounds();
};


struct Genode::Slab::Entry
{
		Block &block;
		char   data[];

		/*
		 * Caution! no member variables allowed below this line!
		 */

		explicit Entry(Block &block) : block(block)
		{
			block.dec_avail();
		}

		~Entry()
		{
			block.inc_avail(*this);
		}

		/**
		 * Lookup Entry by given address
		 *
		 * The specified address is supposed to point to _data[0].
		 */
		static Entry *slab_entry(void *addr) {
			return (Entry *)((addr_t)addr - sizeof(Entry)); }
};


/****************
 ** Slab block **
 ****************/

Slab::Entry *Slab::Block::_slab_entry(int idx)
{
	/*
	 * The slab slots start after the state array that consists
	 * of 'num_elem' bytes. We align the first slot to a four-aligned
	 * address.
	 */

	size_t const entry_size = sizeof(Entry) + _slab._slab_size;
	return (Entry *)&_data[align_addr(_slab._entries_per_block, log2(sizeof(addr_t)))
	                            + entry_size*idx];
}


int Slab::Block::_slab_entry_idx(Slab::Entry *e)
{
	size_t const entry_size = sizeof(Entry) + _slab._slab_size;
	return ((addr_t)e - (addr_t)_slab_entry(0))/entry_size;
}


void *Slab::Block::alloc()
{
	for (unsigned i = 0; i < _slab._entries_per_block; i++) {
		if (_state(i) != FREE)
			continue;

		_state(i, USED);
		Entry * const e = _slab_entry(i);
		construct_at<Entry>(e, *this);
		return e->data;
	}
	return nullptr;
}


Slab::Entry *Slab::Block::any_used_entry()
{
	for (unsigned i = 0; i < _slab._entries_per_block; i++)
		if (_state(i) == USED)
			return _slab_entry(i);

	return nullptr;
}


void Slab::Block::inc_avail(Entry &e)
{
	/* mark slab entry as free */
	int const idx = _slab_entry_idx(&e);
	_state(idx, FREE);
	_avail++;

	/* search previous block with higher avail value than this' */
	Block *at = prev;

	while (at && (at->avail() < _avail))
		at = at->prev;

	/*
	 * If we already are the first block or our avail value is lower than the
	 * previous block, do not reposition the block in the list.
	 */
	if (prev == nullptr || at == prev)
		return;

	/* reposition block in list after block with higher avail value */
	_slab._remove_sb(this);
	_slab._insert_sb(this, at);
}


void Slab::Block::dec_avail()
{
	_avail--;

	/* search subsequent block with lower avail value than this' */
	Block *at = this;

	while (at->next && at->next->avail() > _avail)
		at = at->next;

	if (at == this) return;

	_slab._remove_sb(this);
	_slab._insert_sb(this, at);
}


/**********
 ** Slab **
 **********/

Slab::Slab(size_t slab_size, size_t block_size, void *initial_sb,
           Allocator *backing_store)
:
	_slab_size(slab_size),
	_block_size(block_size),

	/*
	 * Calculate number of entries per slab block.
	 *
	 * The 'sizeof(umword_t)' is for the alignment of the first slab entry.
	 * The 1 is for one byte state entry.
	 */
	_entries_per_block((_block_size - sizeof(Block) - sizeof(umword_t))
	                   / (_slab_size + sizeof(Entry) + 1)),

	_first_sb((Block *)initial_sb),
	_initial_sb(_first_sb),
	_alloc_state(false),
	_backing_store(backing_store)
{
	/* if no initial slab block was specified, try to get one */
	if (!_first_sb && _backing_store)
		_first_sb = _new_slab_block();

	/* init first slab block */
	if (_first_sb)
		construct_at<Block>(_first_sb, *this);
}


Slab::~Slab()
{
	/* free backing store */
	while (_first_sb) {
		Block * const sb = _first_sb;
		_remove_sb(_first_sb);

		/*
		 * Only free slab blocks that we allocated. This is not the case for
		 * the '_initial_sb' that we got as constructor argument.
		 */
		if (_backing_store && (sb != _initial_sb))
			_backing_store->free(sb, _block_size);
	}
}


Slab::Block *Slab::_new_slab_block()
{
	void *sb = nullptr;
	if (!_backing_store || !_backing_store->alloc(_block_size, &sb))
		return nullptr;

	return construct_at<Block>(sb, *this);
}


void Slab::_remove_sb(Block *sb)
{
	Block *prev = sb->prev;
	Block *next = sb->next;

	if (prev) prev->next = next;
	if (next) next->prev = prev;

	if (_first_sb == sb)
		_first_sb =  next;

	sb->prev = sb->next = nullptr;
}


void Slab::_insert_sb(Block *sb, Block *at)
{
	/* determine next-pointer where to assign the current sb */
	Block **nextptr_to_sb = at ? &at->next : &_first_sb;

	/* insert current sb */
	sb->next = *nextptr_to_sb;
	*nextptr_to_sb = sb;

	/* update prev-pointer or succeeding block */
	if (sb->next)
		sb->next->prev = sb;

	sb->prev = at;
}


bool Slab::_num_free_entries_higher_than(int n)
{
	int cnt = 0;

	for (Block *b = _first_sb; b && b->avail() > 0; b = b->next) {
		cnt += b->avail();
		if (cnt > n)
			return true;
	}
	return false;
}


void Slab::insert_sb(void *ptr)
{
	_insert_sb(construct_at<Block>(ptr, *this));
}


bool Slab::alloc(size_t size, void **out_addr)
{
	/* sanity check if first slab block is gone */
	if (!_first_sb) return false;

	/*
	 * If we run out of slab, we need to allocate a new slab block. For the
	 * special case that this block is allocated using the allocator that by
	 * itself uses the slab allocator, such an allocation could cause up to
	 * three new slab_entry allocations. So we need to ensure to allocate the
	 * new slab block early enough - that is if there are only three free slab
	 * entries left.
	 */
	if (_backing_store && !_num_free_entries_higher_than(3) && !_alloc_state) {

		/* allocate new block for slab */
		_alloc_state = true;
		Block *sb = _new_slab_block();
		_alloc_state = false;

		if (!sb) return false;

		/*
		 * The new block has the maximum number of available slots and
		 * so we can insert it at the beginning of the sorted block
		 * list.
		 */
		_insert_sb(sb);
	}

	*out_addr = _first_sb->alloc();
	return *out_addr == nullptr ? false : true;
}


void Slab::_free(void *addr)
{
	Entry *e = addr ? Entry::slab_entry(addr) : nullptr;

	if (e)
		e->~Entry();
}


void *Slab::any_used_elem()
{
	for (Block *b = _first_sb; b; b = b->next) {

		/* skip completely free slab blocks */
		if (b->avail() == _entries_per_block)
			continue;

		/* found a block with used elements - return address of the first one */
		Entry *e = b->any_used_entry();
		if (e) return e->data;
	}
	return nullptr;
}


size_t Slab::consumed() const
{
	/* count number of slab blocks */
	unsigned sb_cnt = 0;
	for (Block *sb = _first_sb; sb; sb = sb->next)
		sb_cnt++;

	return sb_cnt * _block_size;
}
