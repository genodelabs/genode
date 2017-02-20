/*
 * \brief  Heap partition
 * \author Norman Feske
 * \date   2006-05-15
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__HEAP_H_
#define _INCLUDE__BASE__HEAP_H_

#include <util/list.h>
#include <util/reconstructible.h>
#include <ram_session/ram_session.h>
#include <region_map/region_map.h>
#include <base/allocator_avl.h>
#include <base/lock.h>

namespace Genode {
	
	class Heap;
	class Sliced_heap;
}


/**
 * Heap that uses dataspaces as backing store
 *
 * The heap class provides an allocator that uses a list of dataspaces of a RAM
 * session as backing store. One dataspace may be used for holding multiple blocks.
 */
class Genode::Heap : public Allocator
{
	private:

		class Dataspace : public List<Dataspace>::Element
		{
			public:

				Ram_dataspace_capability cap;
				void  *local_addr;
				size_t size;

				Dataspace(Ram_dataspace_capability c, void *local_addr, size_t size)
				: cap(c), local_addr(local_addr), size(size) { }
		};

		/*
		 * This structure exists only to make sure that the dataspaces are
		 * destroyed after the AVL allocator.
		 */
		struct Dataspace_pool : public List<Dataspace>
		{
			Ram_session *ram_session; /* RAM session for backing store */
			Region_map  *region_map;

			Dataspace_pool(Ram_session *ram, Region_map *rm)
			: ram_session(ram), region_map(rm) { }

			~Dataspace_pool();

			void remove_and_free(Dataspace &);

			void reassign_resources(Ram_session *ram, Region_map *rm) {
				ram_session = ram, region_map = rm; }
		};

		Lock                           _lock;
		Reconstructible<Allocator_avl> _alloc;        /* local allocator    */
		Dataspace_pool                 _ds_pool;      /* list of dataspaces */
		size_t                         _quota_limit;
		size_t                         _quota_used;
		size_t                         _chunk_size;

		/**
		 * Allocate a new dataspace of the specified size
		 *
		 * \param size                       number of bytes to allocate
		 * \param enforce_separate_metadata  if true, the new dataspace
		 *                                   will not contain any meta data
		 * \throw                            Region_map::Invalid_dataspace,
		 *                                   Region_map::Region_conflict
		 * \return                           0 on success or negative error code
		 */
		Heap::Dataspace *_allocate_dataspace(size_t size, bool enforce_separate_metadata);

		/**
		 * Try to allocate block at our local allocator
		 *
		 * \return true on success
		 *
		 * This method is a utility used by '_unsynchronized_alloc' to
		 * avoid code duplication.
		 */
		bool _try_local_alloc(size_t size, void **out_addr);

		/**
		 * Unsynchronized implementation of 'alloc'
		 */
		bool _unsynchronized_alloc(size_t size, void **out_addr);

	public:

		enum { UNLIMITED = ~0 };

		Heap(Ram_session *ram_session,
		     Region_map  *region_map,
		     size_t       quota_limit = UNLIMITED,
		     void        *static_addr = 0,
		     size_t       static_size = 0);

		Heap(Ram_session &ram, Region_map &rm) : Heap(&ram, &rm) { }

		~Heap();

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
		void reassign_resources(Ram_session *ram, Region_map *rm) {
			_ds_pool.reassign_resources(ram, rm); }


		/*************************
		 ** Allocator interface **
		 *************************/

		bool   alloc(size_t, void **) override;
		void   free(void *, size_t) override;
		size_t consumed() const override { return _quota_used; }
		size_t overhead(size_t size) const override { return _alloc->overhead(size); }
		bool   need_size_for_free() const override { return false; }
};


/**
 * Heap that allocates each block at a separate dataspace
 */
class Genode::Sliced_heap : public Allocator
{
	private:

		/**
		 * Meta-data header placed in front of each allocated block
		 */
		struct Block : List<Block>::Element
		{
			Ram_dataspace_capability const ds;
			size_t                   const size;

			Block(Ram_dataspace_capability ds, size_t size) : ds(ds), size(size)
			{ }
		};

		Ram_session    &_ram_session;  /* RAM session for backing store   */
		Region_map     &_region_map;   /* region map of the address space */
		size_t          _consumed;     /* number of allocated bytes       */
		List<Block>     _blocks;       /* list of allocated blocks        */
		Lock            _lock;         /* serialize allocations           */

	public:

		/**
		 * Return size of header prepended to each allocated block in bytes
		 */
		static constexpr size_t meta_data_size() { return sizeof(Block); }

		/**
		 * Constructor
		 *
		 * \deprecated  Use the other constructor that takes reference
		 *              arguments
		 */
		Sliced_heap(Ram_session *ram_session, Region_map *region_map)
		: Sliced_heap(*ram_session, *region_map) { }

		/**
		 * Constructor
		 */
		Sliced_heap(Ram_session &ram_session, Region_map &region_map);

		/**
		 * Destructor
		 */
		~Sliced_heap();


		/*************************
		 ** Allocator interface **
		 *************************/

		bool   alloc(size_t, void **);
		void   free(void *, size_t);
		size_t consumed() const { return _consumed; }
		size_t overhead(size_t size) const;
		bool   need_size_for_free() const override { return false; }
};

#endif /* _INCLUDE__BASE__HEAP_H_ */
