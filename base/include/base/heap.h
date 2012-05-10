/*
 * \brief  Heap partition
 * \author Norman Feske
 * \date   2006-05-15
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__HEAP_H_
#define _INCLUDE__BASE__HEAP_H_

#include <util/list.h>
#include <ram_session/ram_session.h>
#include <rm_session/rm_session.h>
#include <base/allocator_avl.h>
#include <base/lock.h>

namespace Genode {

	/**
	 * Heap that uses dataspaces as backing store
	 *
	 * The heap class provides an allocator that uses a list of dataspaces of a ram
	 * session as backing store. One dataspace may be used for holding multiple blocks.
	 */
	class Heap : public Allocator
	{
		private:

			enum {
				MIN_CHUNK_SIZE =    4*1024,  /* in machine words */
				MAX_CHUNK_SIZE = 1024*1024
			};

			class Dataspace : public List<Dataspace>::Element
			{
				public:

					Ram_dataspace_capability cap;
					void  *local_addr;

					Dataspace(Ram_dataspace_capability c, void *a)
					: cap(c), local_addr(a) {}

					inline void * operator new(Genode::size_t, void* addr) {
						return addr; }
					inline void operator delete(void*) { }
			};

			class Dataspace_pool : public List<Dataspace>
			{
				private:

					Ram_session *_ram_session;  /* ram session for backing store */
					Rm_session  *_rm_session;   /* region manager                */

				public:

					/**
					 * Constructor
					 */
					Dataspace_pool(Ram_session *ram_session, Rm_session *rm_session):
						_ram_session(ram_session), _rm_session(rm_session) { }

					/**
					 * Destructor
					 */
					~Dataspace_pool();

					/**
					 * Expand dataspace by specified size
					 *
					 * \param size      number of bytes to add to the dataspace pool
					 * \param md_alloc  allocator to expand. This allocator is also
					 *                  used for meta data allocation (only after
					 *                  being successfully expanded).
					 * \throw           Rm_session::Invalid_dataspace,
					 *                  Rm_session::Region_conflict
					 * \return          0 on success or negative error code
					 */
					int expand(size_t size, Range_allocator *alloc);

					void reassign_resources(Ram_session *ram, Rm_session *rm) {
						_ram_session = ram, _rm_session = rm; }
			};

			/*
			 * NOTE: The order of the member variables is important for
			 *       the calling order of the destructors!
			 */

			Lock           _lock;
			Dataspace_pool _ds_pool;      /* list of dataspaces */
			Allocator_avl  _alloc;        /* local allocator    */
			size_t         _quota_limit;
			size_t         _quota_used;
			size_t         _chunk_size;

			/**
			 * Try to allocate block at our local allocator
			 *
			 * \return true on success
			 *
			 * This function is a utility used by 'alloc' to avoid
			 * code duplication.
			 */
			bool _try_local_alloc(size_t size, void **out_addr);

		public:

			enum { UNLIMITED = ~0 };

			Heap(Ram_session *ram_session,
			     Rm_session  *rm_session,
			     size_t       quota_limit = UNLIMITED,
			     void        *static_addr = 0,
			     size_t       static_size = 0)
			:
				_ds_pool(ram_session, rm_session),
				_alloc(0),
				_quota_limit(quota_limit), _quota_used(0),
				_chunk_size(MIN_CHUNK_SIZE)
			{
				if (static_addr)
					_alloc.add_range((addr_t)static_addr, static_size);
			}

			/**
			 * Reconfigure quota limit
			 *
			 * \return  negative error code if new quota limit is higher than
			 *          currently used quota.
			 */
			int quota_limit(size_t new_quota_limit);

			/**
			 * Re-assign RAM and RM sessions
			 */
			void reassign_resources(Ram_session *ram, Rm_session *rm) {
				_ds_pool.reassign_resources(ram, rm); }


			/*************************
			 ** Allocator interface **
			 *************************/

			bool   alloc(size_t, void **);
			void   free(void *, size_t);
			size_t consumed() { return _quota_used; }
			size_t overhead(size_t size) { return _alloc.overhead(size); }
			bool   need_size_for_free() const { return false; }
	};


	/**
	 * Heap that allocates each block at a separate dataspace
	 */
	class Sliced_heap : public Allocator
	{
		private:

			class Block;

			Ram_session    *_ram_session;  /* ram session for backing store */
			Rm_session     *_rm_session;   /* region manager                */
			size_t          _consumed;     /* number of allocated bytes     */
			List<Block>     _block_list;   /* list of allocated blocks      */
			Lock            _lock;         /* serialize allocations         */

		public:

			/**
			 * Constructor
			 */
			Sliced_heap(Ram_session *ram_session, Rm_session *rm_session);

			/**
			 * Destructor
			 */
			~Sliced_heap();


			/*************************
			 ** Allocator interface **
			 *************************/

			bool   alloc(size_t, void **);
			void   free(void *, size_t);
			size_t consumed() { return _consumed; }
			size_t overhead(size_t size);
			bool   need_size_for_free() const { return false; }
	};
}

#endif /* _INCLUDE__BASE__HEAP_H_ */
