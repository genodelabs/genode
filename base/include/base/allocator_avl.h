/*
 * \brief  Interface of AVL-tree-based allocator
 * \author Norman Feske
 * \date   2006-04-16
 *
 * Each block of the managed address space is present in two AVL trees,
 * one tree ordered by the base addresses of the blocks and one tree ordered
 * by the available capacity within the block.
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__ALLOCATOR_AVL_H_
#define _INCLUDE__BASE__ALLOCATOR_AVL_H_

#include <base/allocator.h>
#include <base/tslab.h>
#include <util/avl_tree.h>
#include <util/misc_math.h>

namespace Genode {

	class Allocator_avl_base : public Range_allocator
	{
		private:

			static bool _sum_in_range(addr_t addr, addr_t offset) {
				return (~0UL - addr > offset); }

		protected:

			class Block : public Avl_node<Block>
			{
				private:

					addr_t _addr;       /* base address    */
					size_t _size;       /* size of block   */
					bool   _used;       /* block is in use */
					short  _id;         /* for debugging   */
					size_t _max_avail;  /* biggest free block size of subtree */

					/**
					 * Request max_avail value of subtree
					 */
					inline size_t _child_max_avail(bool side) {
						return child(side) ? child(side)->max_avail() : 0; }

					/**
					 * Query if block can hold a specified subblock
					 *
					 * \param n       number of bytes
					 * \param align   alignment (power of two)
					 * \return        true if block fits
					 */
					inline bool _fits(size_t n, unsigned align = 1) {
						return ((align_addr(addr(), align) >= addr()) &&
						        _sum_in_range(align_addr(addr(), align), n) &&
						        (align_addr(addr(), align) - addr() + n <= avail())); }

				public:

					/**
					 * Avl_node interface: compare two nodes
					 */
					bool higher(Block *a) {
						return a->_addr >= _addr; }

					/**
					 * Avl_node interface: update meta data on node rearrangement
					 */
					void recompute();

					/**
					 * Accessor functions
					 */
					inline int    id()            { return _id; }
					inline addr_t addr()          { return _addr; }
					inline size_t avail()         { return _used ? 0 : _size; }
					inline size_t size()          { return _size; }
					inline bool   used()          { return _used; }
					inline size_t max_avail()     { return _max_avail; }
					inline void   used(bool used) { _used = used; }

					enum { FREE = false, USED = true };

					/**
					 * Constructor
					 *
					 * This constructor is called from meta-data allocator during
					 * initialization of new meta-data blocks.
					 */
					Block() : _addr(0), _size(0), _used(0), _max_avail(0) { }

					/**
					 * Constructor
					 */
					Block(addr_t addr, size_t size, bool used)
					: _addr(addr), _size(size), _used(used),
					  _max_avail(used ? 0 : size)
					{
						static int num_blocks;
						_id = ++num_blocks;
					}

					/**
					 * Find best-fitting block
					 */
					Block *find_best_fit(size_t size, unsigned align = 1);

					/**
					 * Find block that contains the specified address range
					 */
					Block *find_by_address(addr_t addr, size_t size = 0,
					                       bool check_overlap = 0);

					/**
					 * Return sum of available memory in subtree
					 */
					size_t avail_in_subtree(void);

					/**
					 * Debug hooks
					 */
					void dump();
					void dump_dot(int indent = 0);
			};

		private:

			Avl_tree<Block>  _addr_tree;      /* blocks sorted by base address */
			Allocator       *_md_alloc;       /* meta-data allocator           */
			size_t           _md_entry_size;  /* size of block meta-data entry */

			/**
			 * Alloc meta-data block
			 */
			Block *_alloc_block_metadata();

			/**
			 * Alloc two meta-data blocks in a transactional way
			 */
			bool _alloc_two_blocks_metadata(Block **dst1, Block **dst2);

			/**
			 * Create new block
			 */
			int _add_block(Block *block_metadata,
			               addr_t base, size_t size, bool used);

			/**
			 * Destroy block
			 */
			void _destroy_block(Block *b);

			/**
			 * Cut specified area from block
			 *
			 * The original block gets replaced by (up to) two smaller blocks
			 * with remaining space.
			 */
			void _cut_from_block(Block *b, addr_t cut_addr, size_t cut_size,
			                     Block *dst1, Block *dst2);

		protected:

			/**
			 * Find block by specified address
			 */
			Block *_find_by_address(addr_t addr, size_t size = 0,
			                        bool check_overlap = 0) const
			{
				Block *b = static_cast<Block *>(_addr_tree.first());

				/* if the tree has one or more nodes, start search */
				return b ? b->find_by_address(addr, size, check_overlap) : 0;
			}

			/**
			 * Constructor
			 *
			 * This constructor can only be called from a derived class that
			 * provides an allocator for block meta-data entries. This way,
			 * we can attach custom information to block meta data.
			 */
			Allocator_avl_base(Allocator *md_alloc, size_t md_entry_size) :
				_md_alloc(md_alloc), _md_entry_size(md_entry_size) { }

		public:

			/**
			 * Return address of any block of the allocator
			 *
			 * \param out_addr   result that contains address of block
			 * \return           true if block was found or
			 *                   false if there is no block available
			 *
			 * If no block was found, out_addr is set to zero.
			 */
			bool any_block_addr(addr_t *out_addr);

			/**
			 * Debug hook
			 */
			void dump_addr_tree(Block *addr_node = 0);


			/*******************************
			 ** Range allocator interface **
			 *******************************/

			int          add_range(addr_t base, size_t size);
			int          remove_range(addr_t base, size_t size);
			bool         alloc_aligned(size_t size, void **out_addr, int align = 0);
			Alloc_return alloc_addr(size_t size, addr_t addr);
			void         free(void *addr);
			size_t       avail();
			bool         valid_addr(addr_t addr);


			/*************************
			 ** Allocator interface **
			 *************************/

			bool alloc(size_t size, void **out_addr) {
				return Allocator_avl_base::alloc_aligned(size, out_addr); }

			void free(void *addr, size_t) { free(addr); }

			/**
			 * Return the memory overhead per Block
			 *
			 * The overhead is a rough estimation. If a block is somewhere
			 * in the middle of a free area, we could consider the meta data
			 * for the two free subareas when calculating the overhead.
			 *
			 * The 'sizeof(umword_t)' represents the overhead of the meta-data
			 * slab allocator.
			 */
			size_t overhead(size_t size) { return sizeof(Block) + sizeof(umword_t); }

			bool need_size_for_free() const { return false; }
	};


	/**
	 * AVL-based allocator with custom meta data attached to each block.
	 *
	 * \param BMDT  block meta-data type
	 */
	template <typename BMDT>
	class Allocator_avl_tpl : public Allocator_avl_base
	{
		private:

			enum { SLAB_BLOCK_SIZE = 256 * sizeof(addr_t) };

			/*
			 * Pump up the Block class with custom meta-data type
			 */
			class Block : public Allocator_avl_base::Block, public BMDT { };

			Tslab<Block,SLAB_BLOCK_SIZE> _metadata;  /* meta-data allocator            */
			char _initial_md_block[SLAB_BLOCK_SIZE]; /* first (static) meta-data block */

		public:

			/**
			 * Constructor
			 *
			 * \param metadata_chunk_alloc  pointer to allocator used to allocate
			 *                              meta-data blocks. If set to 0,
			 *                              use ourself for allocating our
			 *                              meta-data blocks. This works only
			 *                              if the managed memory is completely
			 *                              accessible by the allocator.
			 */
			explicit Allocator_avl_tpl(Allocator *metadata_chunk_alloc) :
				Allocator_avl_base(&_metadata, sizeof(Block)),
				_metadata((metadata_chunk_alloc) ? metadata_chunk_alloc : this,
				          (Slab_block *)&_initial_md_block) { }

			/**
			 * Assign custom meta data to block at specified address
			 */
			void metadata(void *addr, BMDT bmd) const
			{
				Block *b = static_cast<Block *>(_find_by_address((addr_t)addr));
				if (b) *static_cast<BMDT *>(b) = bmd;
			}

			/**
			 * Return meta data that was attached to block at specified address
			 */
			BMDT* metadata(void *addr) const
			{
				Block *b = static_cast<Block *>(_find_by_address((addr_t)addr));
				return b && b->used() ? b : 0;
			}

			int add_range(addr_t base, size_t size)
			{
				/*
				 * We disable the slab block allocation while
				 * processing add_range to prevent avalanche
				 * effects when (slab trying to make an allocation
				 * at Allocator_avl that is empty).
				 */
				Allocator *md_bs = _metadata.backing_store();
				_metadata.backing_store(0);
				int ret = Allocator_avl_base::add_range(base, size);
				_metadata.backing_store(md_bs);
				return ret;
			}
	};


	/**
	 * Define AVL-based allocator without any meta data attached to each block
	 */
	class Empty { };
	typedef Allocator_avl_tpl<Empty> Allocator_avl;
}

#endif /* _INCLUDE__BASE__ALLOCATOR_AVL_H_ */
