/*
 * \brief  Interface of AVL-tree-based allocator
 * \author Norman Feske
 * \date   2006-04-16
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__ALLOCATOR_AVL_H_
#define _INCLUDE__BASE__ALLOCATOR_AVL_H_

#include <base/allocator.h>
#include <base/tslab.h>
#include <base/output.h>
#include <util/avl_tree.h>
#include <util/misc_math.h>
#include <util/construct_at.h>

namespace Genode {

	class Allocator_avl_base;

	/*
	 * The default slab block size is dimensioned such that slab-block
	 * allocations make effective use of entire memory pages. To account for
	 * the common pattern of using a 'Sliced_heap' as backing store for the
	 * 'Allocator_avl'. We remove 8 words from the slab-block size to take the
	 * meta-data overhead of each sliced-heap block into account.
	 */
	template <typename, unsigned SLAB_BLOCK_SIZE = (1024 - 8)*sizeof(addr_t)>
	class Allocator_avl_tpl;

	/**
	 * Define AVL-based allocator without any meta data attached to each block
	 */
	class Empty { };
	typedef Allocator_avl_tpl<Empty> Allocator_avl;
}


class Genode::Allocator_avl_base : public Range_allocator
{
	private:

		static bool _sum_in_range(addr_t addr, addr_t offset) {
			return (addr + offset - 1) >= addr; }

		/*
		 * Noncopyable
		 */
		Allocator_avl_base(Allocator_avl_base const &);
		Allocator_avl_base &operator = (Allocator_avl_base const &);

	protected:

		class Block : public Avl_node<Block>
		{
			private:

				addr_t _addr      { 0 };     /* base address    */
				size_t _size      { 0 };     /* size of block   */
				bool   _used      { false }; /* block is in use */
				short  _id        { 0 };     /* for debugging   */
				size_t _max_avail { 0 };     /* biggest free block size of
				                                sub tree */

				/**
				 * Request max_avail value of subtree
				 */
				inline size_t _child_max_avail(bool side) {
					return child(side) ? child(side)->max_avail() : 0; }

				/**
				 * Query if block can hold a specified subblock
				 *
				 * \param n       number of bytes
				 * \param from    minimum start address of subblock
				 * \param to      maximum end address of subblock
				 * \param align   alignment (power of two)
				 * \return        true if block fits
				 */
				inline bool _fits(size_t n, unsigned align,
				                  addr_t from, addr_t to)
				{
					addr_t a = align_addr(addr() < from ? from : addr(),
					                      align);
					return (a >= addr()) && _sum_in_range(a, n) &&
					       (a - addr() + n <= avail()) && (a + n - 1 <= to);
				}

				/*
				 * Noncopyable
				 */
				Block(Block const &);
				Block &operator = (Block const &);

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


				/*****************
				 ** Accessorors **
				 *****************/

				inline int    id()        const { return _id; }
				inline addr_t addr()      const { return _addr; }
				inline size_t avail()     const { return _used ? 0 : _size; }
				inline size_t size()      const { return _size; }
				inline bool   used()      const { return _used; }
				inline size_t max_avail() const { return _max_avail; }

				inline void used(bool used) { _used = used; }

				enum { FREE = false, USED = true };

				/**
				 * Constructor
				 *
				 * This constructor is called from meta-data allocator during
				 * initialization of new meta-data blocks.
				 */
				Block();

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
				Block *find_best_fit(size_t size, unsigned align,
				                     addr_t from = 0UL, addr_t to = ~0UL);

				/**
				 * Find block that contains the specified address range
				 */
				Block *find_by_address(addr_t addr, size_t size = 0,
				                       bool check_overlap = 0);

				/**
				 * Return sum of available memory in subtree
				 */
				size_t avail_in_subtree(void);
		};

	private:

		Avl_tree<Block> _addr_tree        { };  /* blocks sorted by base address */
		Allocator      *_md_alloc { nullptr };  /* meta-data allocator           */
		size_t          _md_entry_size  { 0 };  /* size of block meta-data entry */

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

		Block *_find_any_used_block(Block *sub_tree);

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

		Avl_tree<Block> const & _block_tree() const { return _addr_tree; }

		/**
		 * Clean up the allocator and detect dangling allocations
		 *
		 * This method is called at the destruction time of the allocator. It
		 * makes sure that the allocator instance releases all memory obtained
		 * from the meta-data allocator.
		 */
		void _revert_allocations_and_ranges();

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

		~Allocator_avl_base() { _revert_allocations_and_ranges(); }

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

		void print(Output &out) const;


		/*******************************
		 ** Range allocator interface **
		 *******************************/

		int          add_range(addr_t base, size_t size) override;
		int          remove_range(addr_t base, size_t size) override;
		Alloc_return alloc_aligned(size_t size, void **out_addr, int align,
		                           addr_t from = 0, addr_t to = ~0UL) override;
		Alloc_return alloc_addr(size_t size, addr_t addr) override;
		void         free(void *addr) override;
		size_t       avail() const override;
		bool         valid_addr(addr_t addr) const override;


		/*************************
		 ** Allocator interface **
		 *************************/

		bool alloc(size_t size, void **out_addr) override
		{
			return (Allocator_avl_base::alloc_aligned(
				size, out_addr, log2(sizeof(addr_t))).ok());
		}

		void free(void *addr, size_t) override { free(addr); }

		/**
		 * Return size of block at specified address
		 */
		size_t size_at(void const *addr) const;

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
		size_t overhead(size_t) const override { return sizeof(Block) + sizeof(umword_t); }

		bool need_size_for_free() const override { return false; }
};


/**
 * AVL-based allocator with custom meta data attached to each block.
 *
 * \param BMDT  block meta-data type
 */
template <typename BMDT, unsigned SLAB_BLOCK_SIZE>
class Genode::Allocator_avl_tpl : public Allocator_avl_base
{
	protected:

		/*
		 * Pump up the Block class with custom meta-data type
		 */
		class Block : public Allocator_avl_base::Block, public BMDT { };

		Tslab<Block,SLAB_BLOCK_SIZE> _metadata;  /* meta-data allocator            */
		char _initial_md_block[SLAB_BLOCK_SIZE]; /* first (static) meta-data block */

	public:

		struct Assign_metadata_failed : Exception { };

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
			          (Block *)&_initial_md_block) { }

		~Allocator_avl_tpl() { _revert_allocations_and_ranges(); }

		/**
		 * Return size of slab blocks used for meta data
		 */
		static constexpr size_t slab_block_size() { return SLAB_BLOCK_SIZE; }

		/**
		 * Assign custom meta data to block at specified address
		 *
		 * \throw Assign_metadata_failed
		 */
		void metadata(void *addr, BMDT bmd) const
		{
			Block * const b = static_cast<Block *>(_find_by_address((addr_t)addr));
			if (b) *static_cast<BMDT *>(b) = bmd;
			else throw Assign_metadata_failed();
		}

		/**
		 * Construct meta-data object in place
		 *
		 * \param ARGS  arguments passed to the meta-data constuctor
		 */
		template <typename... ARGS>
		void construct_metadata(void *addr, ARGS &&... args)
		{
			Block * const b = static_cast<Block *>(_find_by_address((addr_t)addr));
			if (b) construct_at<BMDT>(static_cast<BMDT *>(b), args...);
			else throw Assign_metadata_failed();
		}

		/**
		 * Return meta data that was attached to block at specified address
		 */
		BMDT* metadata(void *addr) const
		{
			Block *b = static_cast<Block *>(_find_by_address((addr_t)addr));
			return b && b->used() ? b : 0;
		}

		int add_range(addr_t base, size_t size) override
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

		/**
		 * Apply functor 'fn' to the metadata of an arbitrary
		 * member of the allocator. This method is provided for
		 * destructing each member of the allocator. Calling
		 * the method repeatedly without removing or inserting
		 * members will produce the same member.
		 */
		template <typename FUNC>
		bool apply_any(FUNC const &fn)
		{
			addr_t addr = 0;
			if (any_block_addr(&addr)) {
				if (BMDT *b = metadata((void*)addr)) {
					fn((BMDT&)*b);
					return true;
				}
			}
			return false;
		}
};

#endif /* _INCLUDE__BASE__ALLOCATOR_AVL_H_ */
