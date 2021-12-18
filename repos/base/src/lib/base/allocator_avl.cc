/*
 * \brief  AVL-tree-based memory allocator implementation
 * \author Norman Feske
 * \date   2006-04-18
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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
                                         Range range)
{
	/* find child with lowest max_avail value */
	bool side = _child_max_avail(1) < _child_max_avail(0);

	/* try to find best fitting block in both subtrees */
	for (int i = 0; i < 2; i++, side = !side) {

		if (_child_max_avail(side) < size)
			continue;

		Block *res = child(side) ? child(side)->find_best_fit(size, align, range) : 0;

		if (res)
			return (_fits(size, align, range) && size < res->size()) ? this : res;
	}

	return (_fits(size, align, range)) ? this : 0;
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

Allocator_avl_base::Alloc_md_result Allocator_avl_base::_alloc_block_metadata()
{
	return _md_alloc.try_alloc(sizeof(Block)).convert<Alloc_md_result>(
		[&] (void *ptr) {
			return construct_at<Block>(ptr, 0U, 0U, 0); },
		[&] (Alloc_error error) {
			return error; });
}


Allocator_avl_base::Alloc_md_two_result
Allocator_avl_base::_alloc_two_blocks_metadata()
{
	return _alloc_block_metadata().convert<Alloc_md_two_result>(

		[&] (Block *b1_ptr) {
			return _alloc_block_metadata().convert<Alloc_md_two_result>(

				[&] (Block *b2_ptr) {
					return Two_blocks { b1_ptr, b2_ptr }; },

				[&] (Alloc_error error) {
					_md_alloc.free(b1_ptr, sizeof(Block));
					return error; });
		},
		[&] (Alloc_error error) {
			return error; });
}


void Allocator_avl_base::_add_block(Block &block_metadata,
                                   addr_t base, size_t size, bool used)
{
	/* call constructor for new block */
	construct_at<Block>(&block_metadata, base, size, used);

	/* insert block into avl tree */
	_addr_tree.insert(&block_metadata);
}


void Allocator_avl_base::_destroy_block(Block &b)
{
	_addr_tree.remove(&b);
	_md_alloc.free(&b, _md_entry_size);
}


void Allocator_avl_base::_cut_from_block(Block &b, addr_t addr, size_t size, Two_blocks blocks)
{
	size_t const padding   = addr > b.addr() ? addr - b.addr() : 0;
	size_t const b_size    = b.size() > padding ? b.size() - padding : 0;
	size_t       remaining = b_size > size ? b_size - size : 0;

	/* case that a block contains the whole addressable range */
	if (!b.addr() && !b.size())
		remaining = b.size() - size - padding;

	addr_t orig_addr = b.addr();

	_destroy_block(b);

	/* create free block containing the alignment padding */
	if (padding > 0)
		_add_block(*blocks.b1_ptr, orig_addr, padding, Block::FREE);
	else
		_md_alloc.free(blocks.b1_ptr, sizeof(Block));

	/* create free block for remaining space of original block */
	if (remaining > 0)
		_add_block(*blocks.b2_ptr, addr + size, remaining, Block::FREE);
	else
		_md_alloc.free(blocks.b2_ptr, sizeof(Block));
}


template <typename FN>
void Allocator_avl_base::_revert_block_ranges(FN const &any_block_fn)
{
	for (bool loop = true; loop; ) {

		Block *block_ptr = any_block_fn();
		if (!block_ptr)
				break;

		remove_range(block_ptr->addr(), block_ptr->size()).with_error(
			[&] (Alloc_error error) {
				if (error == Alloc_error::DENIED) /* conflict */
					_destroy_block(*block_ptr);
				else
					loop = false; /* give up on OUT_OF_RAM or OUT_OF_CAPS */
			});
	}
}


void Allocator_avl_base::_revert_unused_ranges()
{
	_revert_block_ranges([&] () {
		return _find_any_unused_block(_addr_tree.first()); });
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

	/* destroy all remaining blocks */
	_revert_block_ranges([&] () { return _addr_tree.first(); });
}


Allocator_avl_base::Range_result Allocator_avl_base::add_range(addr_t new_addr, size_t new_size)
{
	if (!new_size)
		return Alloc_error::DENIED;

	/* check for conflicts with existing blocks */
	if (_find_by_address(new_addr, new_size, true))
		return Alloc_error::DENIED;

	return _alloc_block_metadata().convert<Range_result>(

		[&] (Block *new_block_ptr) {

			/* merge with predecessor */
			Block *b = nullptr;
			if (new_addr != 0 && (b = _find_by_address(new_addr - 1)) && !b->used()) {

				new_size += b->size();
				new_addr  = b->addr();
				_destroy_block(*b);
			}

			/* merge with successor */
			if ((b = _find_by_address(new_addr + new_size)) && !b->used()) {

				new_size += b->size();
				_destroy_block(*b);
			}

			/* create new block that spans over all merged blocks */
			_add_block(*new_block_ptr, new_addr, new_size, Block::FREE);

			return Range_ok();
		},
		[&] (Alloc_error error) {
			return error; });
}


Allocator_avl_base::Range_result Allocator_avl_base::remove_range(addr_t base, size_t size)
{
	Range_result result = Alloc_error::DENIED;

	if (!size)
		return result;

	for (bool done = false; !done; ) {

		_alloc_two_blocks_metadata().with_result(
			[&] (Two_blocks blocks) {

				/* find block overlapping the specified range */
				Block *b = _addr_tree.first();
				b = b ? b->find_by_address(base, size, 1) : 0;

				/*
				 * If there are no overlappings with any existing blocks (b == 0), we
				 * are done.  If however, the overlapping block is in use, we have a
				 * problem. Stop iterating in both cases.
				 */
				if (!b || !b->avail()) {
					_md_alloc.free(blocks.b1_ptr, sizeof(Block));
					_md_alloc.free(blocks.b2_ptr, sizeof(Block));

					if (b == 0)
						result = Range_ok();

					done = true;
					return;
				}

				/* cut intersecting address range */
				addr_t intersect_beg = max(base, b->addr());
				size_t intersect_end = min(base + size - 1, b->addr() + b->size() - 1);

				_cut_from_block(*b, intersect_beg, intersect_end - intersect_beg + 1, blocks);
			},
			[&] (Alloc_error error) {
				result = error;
				done   = true;
			});
	}
	return result;
}


template <typename SEARCH_FN>
Allocator::Alloc_result
Allocator_avl_base::_allocate(size_t const size, unsigned align, Range range,
                              SEARCH_FN const &search_fn)
{
	return _alloc_two_blocks_metadata().convert<Alloc_result>(

		[&] (Two_blocks two_blocks) -> Alloc_result {

			/* find block according to the policy implemented by 'search_fn' */
			Block *b_ptr = _addr_tree.first();
			b_ptr = b_ptr ? search_fn(*b_ptr) : 0;

			if (!b_ptr) {
				/* range conflict */
				_md_alloc.free(two_blocks.b1_ptr, sizeof(Block));
				_md_alloc.free(two_blocks.b2_ptr, sizeof(Block));
				return Alloc_error::DENIED;
			}
			Block &b = *b_ptr;

			/* calculate address of new (aligned) block */
			addr_t const new_addr = align_addr(max(b.addr(), range.start), align);

			/* remove new block from containing block, consume two_blocks */
			_cut_from_block(b, new_addr, size, two_blocks);

			/* create allocated block */
			return _alloc_block_metadata().convert<Alloc_result>(

				[&] (Block *new_block_ptr) {
					_add_block(*new_block_ptr, new_addr, size, Block::USED);
					return reinterpret_cast<void *>(new_addr); },

				[&] (Alloc_error error) {
					return error; });
		},
		[&] (Alloc_error error) {
			return error; });
}


Allocator::Alloc_result
Allocator_avl_base::alloc_aligned(size_t size, unsigned align, Range range)
{
	return _allocate(size, align, range, [&] (Block &first) {
			return first.find_best_fit(size, align, range); });
}


Range_allocator::Alloc_result Allocator_avl_base::alloc_addr(size_t size, addr_t addr)
{
	/* check for integer overflow */
	if (addr + size - 1 < addr)
		return Alloc_error::DENIED;

	/* check for range conflict */
	if (!_sum_in_range(addr, size))
		return Alloc_error::DENIED;

	Range    const range { .start = addr, .end = addr + size - 1 };
	unsigned const align_any = 0;

	return _allocate(size, align_any, range, [&] (Block &first) {
			return first.find_by_address(addr, size); });
}


void Allocator_avl_base::free(void *addr)
{
	/* lookup corresponding block */
	Block *b = _find_by_address(reinterpret_cast<addr_t>(addr));

	if (!b || !(b->used())) return;

	addr_t const new_addr = b->addr();
	size_t const new_size = b->size();

	if (new_addr != (addr_t)addr)
		error(__PRETTY_FUNCTION__, ": given address (", addr, ") "
		      "is not the block start address (", (void *)new_addr, ")");

	_destroy_block(*b);

	add_range(new_addr, new_size);
}


size_t Allocator_avl_base::size_at(void const *addr) const
{
	/* lookup corresponding block */
	Block *b = _find_by_address(reinterpret_cast<addr_t>(addr));

	if (b && (b->addr() != (addr_t)addr))
		return 0;

	return (b && b->used()) ? b->size() : 0;
}


Allocator_avl_base::Block *Allocator_avl_base::_find_any_unused_block(Block *sub_tree)
{
	if (!sub_tree)
		return nullptr;

	if (!sub_tree->used())
		return sub_tree;

	for (unsigned i = 0; i < 2; i++)
		if (Block *block = _find_any_unused_block(sub_tree->child(i)))
			return block;

	return nullptr;
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
