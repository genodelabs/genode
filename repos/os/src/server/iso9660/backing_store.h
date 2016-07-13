/*
 * \brief  Backing store for on-demand-paged managed dataspaces
 * \author Norman Feske
 * \date   2010-10-31
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BACKING_STORE_H_
#define _BACKING_STORE_H_

#include <base/env.h>

/**
 * Fifo-based physical backing-store allocator
 *
 * \param UMD  user-specific metadata attached to each backing-store block,
 *             for example the corresponding offset within a managed
 *             dataspace
 */
template <typename UMD>
class Backing_store
{
	public:

		/**
		 * Interface to be implemented by the user of the backing store
		 */
		struct User
		{
			virtual void detach_block(UMD umd)
			{
				Genode::warning(__func__, " this should never be called");
			}
		};

		/**
		 * Pseudo user for marking a block as used during the
		 * time between the allocation and the assignment to the
		 * actual user
		 */
		User _not_yet_assigned;

		/**
		 * Meta data of a backing-store block
		 */
		class Block
		{
			private:

				friend class Backing_store;

				/**
				 * User of the block, 0 if block is unused
				 */
				User *_user;

				/**
				 * User-specific meta data. It is not used by the
				 * 'Backing_store', just handed back to the user with the
				 * 'detach_block' function.
				 */
				UMD _user_meta_data;

				/**
				 * Default constructor used for array allocation
				 */
				Block() : _user(0) { }

				/**
				 * Used by 'Backing_store::assign'
				 */
				void assign_user(User *user, UMD user_meta_data) {
					_user = user, _user_meta_data = user_meta_data; }

				/**
				 * Used by 'Backing_store::alloc'
				 */
				void assign_pseudo_user(User *user) { _user = user; }

				/**
				 * Return true if block is in use
				 */
				bool occupied() const { return _user != 0; }

				/**
				 * Return user of the block
				 */
				const User *user() const { return _user; }

				/**
				 * Evict block, preparing to for reuse
				 */
				void evict()
				{
					if (_user)
						_user->detach_block(_user_meta_data);
					_user = 0;
				}
		};

	private:

		/**
		 * Lock for synchronizing the block allocator
		 */
		Genode::Lock _alloc_lock;

		const Genode::size_t _block_size;

		/**
		 * Number of physical blocks
		 */
		const Genode::size_t _num_blocks;

		/**
		 * RAM dataspace holding the physical backing store
		 */
		const Genode::Dataspace_capability _ds;

		/**
		 * Local address of physical backing store
		 */
		const void *_ds_addr;

		/**
		 * Array of block meta data
		 */
		Block *_blocks;

		/**
		 * Block index for next allocation (used for fifo)
		 */
		unsigned long _curr_block_idx;

		/**
		 * Return meta data of next-to-be-allocated block
		 */
		Block *_curr_block() const { return &_blocks[_curr_block_idx]; }

		/**
		 * Advance fifo allocation index
		 */
		void _advance_curr_block()
		{
			_curr_block_idx++;
			if (_curr_block_idx == _num_blocks)
				_curr_block_idx = 0;
		}

		/**
		 * Calculate number of blocks that fit into specified amount of RAM,
		 * taking the costs for meta data into account
		 */
		Genode::size_t _calc_num_blocks(Genode::size_t ram_size) const
		{
			return ram_size / (sizeof(Block) + _block_size);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param ram_size    number of bytes of physical RAM to be used as
		 *                    backing store for both the meta data and the
		 *                    payload
		 */
		Backing_store(Genode::size_t ram_size, Genode::size_t block_size)
		:
			_block_size(block_size),
			_num_blocks(_calc_num_blocks(ram_size)),
			_ds(Genode::env()->ram_session()->alloc(_block_size*_num_blocks)),
			_ds_addr(Genode::env()->rm_session()->attach(_ds)),
			_blocks(new (Genode::env()->heap()) Block[_num_blocks]),
			_curr_block_idx(0)
		{ }

		Block *alloc()
		{
			Genode::Lock::Guard guard(_alloc_lock);

			/* skip blocks that are currently in the process of being assigned */
			while (_curr_block()->user() == &_not_yet_assigned) {
				_advance_curr_block();
			}

			/* evict block if needed */
			if (_curr_block()->occupied())
				_curr_block()->evict();

			/* reserve allocated block (prevent eviction prior assignment) */
			Block *block = _curr_block();
			block->assign_pseudo_user(&_not_yet_assigned);

			_advance_curr_block();
			return block;
		}

		/**
		 * Return dataspace containing the backing store payload
		 */
		Genode::Dataspace_capability dataspace() const { return _ds; }

		/**
		 * Return block size used by the backing store
		 */
		Genode::size_t block_size() const { return _block_size; }

		/**
		 * Return block index of specified block
		 */
		Genode::off_t index(const Block *b) const { return b - _blocks; }

		/**
		 * Return offset of block within physical backing store
		 */
		Genode::off_t offset(const Block *b) const {
			return index(b)*_block_size; }

		/**
		 * Return local address of physical backing-store block
		 */
		void *local_addr(const Block *b) const {
			return (void *)((Genode::addr_t)_ds_addr + offset(b)); }

		/**
		 * Assign final user of a block
		 *
		 * After calling this function, the block will be subjected to
		 * eviction, if needed.
		 */
		void assign(Block *block, User *user, UMD user_meta_data)
		{
			Genode::Lock::Guard guard(_alloc_lock);
			block->assign_user(user, user_meta_data);
		}

		/**
		 * Evict all blocks currently in use by the specified user
		 */
		void flush(const User *user)
		{
			Genode::Lock::Guard guard(_alloc_lock);
			for (unsigned i = 0; i < _num_blocks; i++)
				if (_blocks[i].user() == user)
					_blocks[i].evict();
		}
};

#endif /* _BACKING_STORE_H_ */
