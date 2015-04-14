/*
 * \brief  Slab allocator implementation
 * \author Norman Feske
 * \date   2006-05-16
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/slab.h>
#include <util/misc_math.h>

using namespace Genode;


/****************
 ** Slab block **
 ****************/

/**
 * Placement operator - tool for directly calling a constructor
 */
inline void *operator new(size_t, void *at) { return at; }


void Slab_block::slab(Slab *slab)
{
	_slab  = slab;
	_avail = _slab->num_elem();
	next   = prev = 0;

	for (unsigned i = 0; i < _avail; i++)
		state(i, FREE);
}


Slab_entry *Slab_block::slab_entry(int idx)
{
	/*
	 * The slab slots start after the state array that consists
	 * of 'num_elem' bytes. We align the first slot to a four-aligned
	 * address.
	 */
	return (Slab_entry *)&_data[align_addr(_slab->num_elem(), 2)
	                            + _slab->entry_size()*idx];
}


int Slab_block::slab_entry_idx(Slab_entry *e) {
	return ((addr_t)e - (addr_t)slab_entry(0))/_slab->entry_size(); }


void *Slab_block::alloc()
{
	size_t num_elem = _slab->num_elem();
	for (unsigned i = 0; i < num_elem; i++)
		if (state(i) == FREE) {
			state(i, USED);
			Slab_entry *e = slab_entry(i);
			e->occupy(this);
			return e->addr();
		}
	return 0;
}


Slab_entry *Slab_block::first_used_entry()
{
	size_t num_elem = _slab->num_elem();
	for (unsigned i = 0; i < num_elem; i++)
		if (state(i) == USED)
			return slab_entry(i);
	return 0;
}


void Slab_block::inc_avail(Slab_entry *e)
{
	/* mark slab entry as free */
	int idx = slab_entry_idx(e);
	state(idx, FREE);
	_avail++;

	/* search previous block with higher avail value than this' */
	Slab_block *at = prev;

	while (at && (at->avail() < _avail))
		at = at->prev;

	/*
	 * If we already are the first block or our avail value is lower than the
	 * previous block, do not reposition the block in the list.
	 */
	if (prev == 0 || at == prev)
		return;

	/* reposition block in list after block with higher avail value */
	_slab->remove_sb(this);
	_slab->insert_sb(this, at);
}


void Slab_block::dec_avail()
{
	_avail--;

	/* search subsequent block with lower avail value than this' */
	Slab_block *at = this;

	while (at->next && at->next->avail() > _avail)
		at = at->next;

	if (at == this) return;

	_slab->remove_sb(this);
	_slab->insert_sb(this, at);
}


/**********
 ** Slab **
 **********/

Slab::Slab(size_t slab_size, size_t block_size, Slab_block *initial_sb,
                                                Allocator *backing_store)
: _slab_size(slab_size),
  _block_size(block_size),
  _first_sb(initial_sb),
  _initial_sb(initial_sb),
  _alloc_state(false),
  _backing_store(backing_store)
{
	/*
	 * Calculate number of entries per slab block.
	 *
	 * The 'sizeof(umword_t)' is for the alignment of the first slab entry.
	 * The 1 is for one byte state entry.
	 */
	_num_elem = (_block_size - sizeof(Slab_block) - sizeof(umword_t))
	          / (entry_size() + 1);

	/* if no initial slab block was specified, try to get one */
	if (!_first_sb && _backing_store)
		_first_sb = _new_slab_block();

	/* init first slab block */
	if (_first_sb)
		_first_sb->slab(this);
}


Slab::~Slab()
{
	/* free backing store */
	while (_first_sb) {
		Slab_block *sb = _first_sb;
		remove_sb(_first_sb);

		/*
		 * Only free slab blocks that we allocated. This is not the case for
		 * the '_initial_sb' that we got as constructor argument.
		 */
		if (_backing_store && (sb != _initial_sb))
			_backing_store->free(sb, _block_size);
	}
}


Slab_block *Slab::_new_slab_block()
{
	void *sb = 0;
	if (!_backing_store || !_backing_store->alloc(_block_size, &sb))
		return 0;

	/* call constructor by using the placement new operator */
	return new(sb) Slab_block(this);
}


void Slab::remove_sb(Slab_block *sb)
{
	Slab_block *prev = sb->prev;
	Slab_block *next = sb->next;

	if (prev) prev->next = next;
	if (next) next->prev = prev;

	if (_first_sb == sb)
		_first_sb =  next;

	sb->prev = sb->next = 0;
}


void Slab::insert_sb(Slab_block *sb, Slab_block *at)
{
	/* determine next-pointer where to assign the current sb */
	Slab_block **nextptr_to_sb = at ? &at->next : &_first_sb;

	/* insert current sb */
	sb->next = *nextptr_to_sb;
	*nextptr_to_sb = sb;

	/* update prev-pointer or succeeding block */
	if (sb->next)
		sb->next->prev = sb;

	sb->prev = at;
}


bool Slab::num_free_entries_higher_than(int n)
{
	int cnt = 0;

	for (Slab_block *b = _first_sb; b && b->avail() > 0; b = b->next) {
		cnt += b->avail();
		if (cnt > n)
			return true;
	}
	return false;
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
	if (_backing_store && !num_free_entries_higher_than(3) && !_alloc_state) {

		/* allocate new block for slab */
		_alloc_state = true;
		Slab_block *sb = _new_slab_block();
		_alloc_state = false;

		if (!sb) return false;

		/*
		 * The new block has the maximum number of available slots and
		 * so we can insert it at the beginning of the sorted block
		 * list.
		 */
		insert_sb(sb);
	}

	*out_addr = _first_sb->alloc();
	return *out_addr == 0 ? false : true;
}


void Slab::free(void *addr)
{
	Slab_entry *e = addr ? Slab_entry::slab_entry(addr) : 0;

	if (e) e->free();
}


void *Slab::first_used_elem()
{
	for (Slab_block *b = _first_sb; b; b = b->next) {

		/* skip completely free slab blocks */
		if (b->avail() == _num_elem)
			continue;

		/* found a block with used elements - return address of the first one */
		Slab_entry *e = b->first_used_entry();
		if (e) return e->addr();
	}
	return 0;
}


size_t Slab::consumed() const
{
	/* count number of slab blocks */
	unsigned sb_cnt = 0;
	for (Slab_block *sb = _first_sb; sb; sb = sb->next)
		sb_cnt++;

	return sb_cnt * _block_size;
}
