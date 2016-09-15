/*
 * \brief  AVL-tree-based memory allocator implementation
 * \author Norman Feske
 * \date   2006-04-18
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <util/construct_at.h>
#include <base/allocator_avl.h>
#include <base/log.h>

using namespace Genode;


/**************************
 ** Block Implementation **
 **************************/

Allocator_avl_base::Block *
Allocator_avl_base::Block::find_best_fit(size_t size, unsigned align,
                                         addr_t from, addr_t to)
{
	/* find child with lowest max_avail value */
	bool side = _child_max_avail(1) < _child_max_avail(0);

	/* try to find best fitting block in both subtrees */
	for (int i = 0; i < 2; i++, side = !side) {

		if (_child_max_avail(side) < size)
			continue;

		Block *res = child(side) ? child(side)->find_best_fit(size, align, from, to) : 0;

		if (res)
			return (_fits(size, align, from, to) && size < res->size()) ? this : res;
	}

	return (_fits(size, align, from, to)) ? this : 0;
}


Allocator_avl_base::Block *
Allocator_avl_base::Block::find_by_address(addr_t find_addr, size_t find_size,
                                           bool check_overlap)
{
	/* the following checks do not work for size==0 */
	find_size = find_size ? find_size : 1;

	/* check for overlapping */
	if (check_overlap
	 && (find_addr + find_size - 1 >= addr())
	 && (addr()    + size()    - 1 >= find_addr))
		return this;

	/* check for containment */
	if ((find_addr >= addr())
	 && (find_addr + find_size - 1 <= addr() + size() - 1))
		return this;

	/* walk into subtree (right if search addr is higher than current */
	Block *c = child(find_addr >= addr());

	/* if such a subtree exists, follow it */
	return c ? c->find_by_address(find_addr, find_size, check_overlap) : 0;
}


size_t Allocator_avl_base::Block::avail_in_subtree(void)
{
	size_t ret = avail();

	/* accumulate subtrees of children */
	for (int i = 0; i < 2; i++)
		if (child(i))
			ret += child(i)->avail_in_subtree();

	return ret;
}


void Allocator_avl_base::Block::recompute()
{
	_max_avail = max(_child_max_avail(0), _child_max_avail(1));
	_max_avail = max(avail(), _max_avail);
}


/**********************************
 ** Allocator_avl implementation **
 **********************************/

Allocator_avl_base::Block *Allocator_avl_base::_alloc_block_metadata()
{
	void *b = nullptr;
	if (_md_alloc->alloc(sizeof(Block), &b))
		return construct_at<Block>(b, 0, 0, 0);

	return nullptr;
}


bool Allocator_avl_base::_alloc_two_blocks_metadata(Block **dst1, Block **dst2)
{
	*dst1 = _alloc_block_metadata();
	*dst2 = _alloc_block_metadata();

	if (!*dst1 && *dst2) _md_alloc->free(*dst2, sizeof(Block));
	if (!*dst2 && *dst1) _md_alloc->free(*dst1, sizeof(Block));

	return (*dst1 && *dst2);
}


int Allocator_avl_base::_add_block(Block *block_metadata,
                                   addr_t base, size_t size, bool used)
{
	if (!block_metadata)
		return -1;

	/* call constructor for new block */
	construct_at<Block>(block_metadata, base, size, used);

	/* insert block into avl tree */
	_addr_tree.insert(block_metadata);

	return 0;
}


void Allocator_avl_base::_destroy_block(Block *b)
{
	if (!b) return;

	/* remove block from both avl trees */
	_addr_tree.remove(b);
	_md_alloc->free(b, _md_entry_size);
}


void Allocator_avl_base::_cut_from_block(Block *b, addr_t addr, size_t size,
                                         Block *dst1, Block *dst2)
{
	size_t   padding = addr > b->addr() ? addr - b->addr() : 0;
	size_t remaining = b->size() > (size + padding) ? b->size() - size - padding : 0;
	addr_t orig_addr = b->addr();

	_destroy_block(b);

	/* create free block containing the alignment padding */
	if (padding > 0)
		_add_block(dst1, orig_addr, padding, Block::FREE);
	else
		_md_alloc->free(dst1, sizeof(Block));

	/* create free block for remaining space of original block */
	if (remaining > 0)
		_add_block(dst2, addr + size, remaining, Block::FREE);
	else
		_md_alloc->free(dst2, sizeof(Block));
}


void Allocator_avl_base::_revert_allocations_and_ranges()
{
	/* revert all allocations */
	size_t dangling_allocations = 0;
	for (;; dangling_allocations++) {
		addr_t addr = 0;
		if (!any_block_addr(&addr))
			break;

		free((void *)addr);
	}

	if (dangling_allocations)
		warning(dangling_allocations, " dangling allocation",
		        (dangling_allocations > 1) ? "s" : "",
		        " at allocator destruction time");

	/* remove ranges */
	while (Block *block = _addr_tree.first())
		remove_range(block->addr(), block->size());
}


int Allocator_avl_base::add_range(addr_t new_addr, size_t new_size)
{
	Block *b;

	/* sanity check for insane users ;-) */
	if (!new_size) return -2;

	/* check for conflicts with existing blocks */
	if (_find_by_address(new_addr, new_size, true))
		return -3;

	Block *new_block = _alloc_block_metadata();
	if (!new_block) return -4;

	/* merge with predecessor */
	if (new_addr != 0 && (b = _find_by_address(new_addr - 1)) && !b->used()) {

		new_size += b->size();
		new_addr  = b->addr();

		_destroy_block(b);
	}

	/* merge with successor */
	if ((b = _find_by_address(new_addr + new_size)) && !b->used()) {

		new_size += b->size();

		_destroy_block(b);
	}

	/* create new block that spans over all merged blocks */
	return _add_block(new_block, new_addr, new_size, Block::FREE);
}


int Allocator_avl_base::remove_range(addr_t base, size_t size)
{
	/* sanity check for insane users ;-) */
	if (!size) return -1;

	Block *dst1, *dst2;
	if (!_alloc_two_blocks_metadata(&dst1, &dst2))
		return -2;

	/* FIXME removing ranges from allocators with used blocks is not safe! */
	while (1) {

		/* find block overlapping the specified range */
		Block *b = _addr_tree.first();
		b = b ? b->find_by_address(base, size, 1) : 0;

		/*
		 * If there are no overlappings with any existing blocks (b == 0), we
		 * are done.  If however, the overlapping block is in use, we have a
		 * problem. In both cases, return.
		 */
		if (!b || !b->avail()) {
			_md_alloc->free(dst1, sizeof(Block));
			_md_alloc->free(dst2, sizeof(Block));
			return !b ? 0 : -3;
		}

		/* cut intersecting address range */
		addr_t intersect_beg = max(base, b->addr());
		size_t intersect_end = min(base + size - 1, b->addr() + b->size() - 1);

		_cut_from_block(b, intersect_beg, intersect_end - intersect_beg + 1, dst1, dst2);
		if (!_alloc_two_blocks_metadata(&dst1, &dst2))
			return -4;
	};
}


Range_allocator::Alloc_return
Allocator_avl_base::alloc_aligned(size_t size, void **out_addr, int align,
                                  addr_t from, addr_t to)
{
	Block *dst1, *dst2;
	if (!_alloc_two_blocks_metadata(&dst1, &dst2))
		return Alloc_return(Alloc_return::OUT_OF_METADATA);

	/* find best fitting block */
	Block *b = _addr_tree.first();
	b = b ? b->find_best_fit(size, align, from, to) : 0;

	if (!b) {
		_md_alloc->free(dst1, sizeof(Block));
		_md_alloc->free(dst2, sizeof(Block));
		return Alloc_return(Alloc_return::RANGE_CONFLICT);
	}

	/* calculate address of new (aligned) block */
	addr_t new_addr = align_addr(b->addr() < from ? from : b->addr(), align);

	/* remove new block from containing block */
	_cut_from_block(b, new_addr, size, dst1, dst2);

	/* create allocated block */
	Block *new_block = _alloc_block_metadata();
	if (!new_block) {
		_md_alloc->free(new_block, sizeof(Block));
		return Alloc_return(Alloc_return::OUT_OF_METADATA);
	}
	_add_block(new_block, new_addr, size, Block::USED);

	*out_addr = reinterpret_cast<void *>(new_addr);
	return Alloc_return(Alloc_return::OK);
}


Range_allocator::Alloc_return Allocator_avl_base::alloc_addr(size_t size, addr_t addr)
{
	/* sanity check */
	if (!_sum_in_range(addr, size))
		return Alloc_return(Alloc_return::RANGE_CONFLICT);

	Block *dst1, *dst2;
	if (!_alloc_two_blocks_metadata(&dst1, &dst2))
		return Alloc_return(Alloc_return::OUT_OF_METADATA);

	/* find block at specified address */
	Block *b = _addr_tree.first();
	b = b ? b->find_by_address(addr, size) : 0;

	/* skip if there's no block or block is used */
	if (!b || b->used()) {
		_md_alloc->free(dst1, sizeof(Block));
		_md_alloc->free(dst2, sizeof(Block));
		return Alloc_return(Alloc_return::RANGE_CONFLICT);
	}

	/* remove new block from containing block */
	_cut_from_block(b, addr, size, dst1, dst2);

	/* create allocated block */
	Block *new_block = _alloc_block_metadata();
	if (!new_block) {
		_md_alloc->free(new_block, sizeof(Block));
		return Alloc_return(Alloc_return::OUT_OF_METADATA);
	}
	_add_block(new_block, addr, size, Block::USED);

	return Alloc_return(Alloc_return::OK);
}


void Allocator_avl_base::free(void *addr)
{
	/* lookup corresponding block */
	Block *b = _find_by_address(reinterpret_cast<addr_t>(addr));

	if (!b || !(b->used())) return;

	addr_t new_addr = b->addr();
	size_t new_size = b->size();

	if (new_addr != (addr_t)addr)
		error(__PRETTY_FUNCTION__, ": given address (", addr, ") "
		      "is not the block start address (", (void *)new_addr, ")");

	_destroy_block(b);

	add_range(new_addr, new_size);
}


size_t Allocator_avl_base::size_at(void const *addr) const
{
	/* lookup corresponding block */
	Block *b = _find_by_address(reinterpret_cast<addr_t>(addr));

	return (b && b->used()) ? b->size() : 0;
}


Allocator_avl_base::Block *Allocator_avl_base::_find_any_used_block(Block *sub_tree)
{
	if (!sub_tree)
		return nullptr;

	if (sub_tree->used())
		return sub_tree;

	for (unsigned i = 0; i < 2; i++)
		if (Block *block = _find_any_used_block(sub_tree->child(i)))
			return block;

	return nullptr;
}


bool Allocator_avl_base::any_block_addr(addr_t *out_addr)
{
	Block * const b = _find_any_used_block(_addr_tree.first());
	*out_addr = b ? b->addr() : 0;
	return b != nullptr;
}


size_t Allocator_avl_base::avail() const
{
	Block *b = static_cast<Block *>(_addr_tree.first());
	return b ? b->avail_in_subtree() : 0;
}


bool Allocator_avl_base::valid_addr(addr_t addr) const
{
	Block *b = _find_by_address(addr);
	return b ? true : false;
}
