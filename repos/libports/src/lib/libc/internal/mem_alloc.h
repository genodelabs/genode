/*
 * \brief  Allocator for anonymous memory used by libc
 * \author Norman Feske
 * \date   2012-05-18
 */

/*
 * Copyright (C) 2012-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__MEM_ALLOC_H_
#define _LIBC__INTERNAL__MEM_ALLOC_H_

/* Genode includes */
#include <base/ram_allocator.h>
#include <base/allocator_avl.h>
#include <base/env.h>
#include <util/list.h>
#include <rm_session/rm_session.h>

/* libc-internal includes */
#include <internal/types.h>

namespace Libc {

	struct Mem_alloc
	{
		virtual void *alloc(size_t size, size_t align_log2) = 0;
		virtual void free(void *ptr) = 0;
		virtual size_t size_at(void const *ptr) const = 0;
	};

	/**
	 * Return singleton instance of the memory allocator
	 */
	Mem_alloc *mem_alloc(bool executable = false);

	class Mem_alloc_impl : public Mem_alloc
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
					void *local_addr;

					Dataspace(Ram_dataspace_capability c, void *a)
					: cap(c), local_addr(a) {}

					inline void * operator new(__SIZE_TYPE__, void* addr) {
						return addr; }

					inline void operator delete(void*) { }
			};

			class Dataspace_pool : public List<Dataspace>
			{
				private:

					Ram_allocator *_ram;          /* RAM session for backing store */
					Region_map    *_region_map;   /* region map of address space   */
					bool const     _executable;   /* whether to allocate executable dataspaces */

				public:

					/**
					 * Constructor
					 */
					Dataspace_pool(Ram_allocator *ram,
					               Region_map *rm, bool executable) :
						_ram(ram), _region_map(rm),
						_executable(executable)
					{ }

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
					 * \throw           Region_map::Invalid_dataspace,
					 *                  Region_map::Region_conflict
					 * \return          0 on success or negative error code
					 */
					int expand(size_t size, Range_allocator *alloc);

					void reassign_resources(Ram_allocator *ram, Region_map *rm) {
						_ram = ram, _region_map = rm; }
			};

			Lock   mutable _lock;
			Dataspace_pool _ds_pool;      /* list of dataspaces */
			Allocator_avl  _alloc;        /* local allocator    */
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

			Mem_alloc_impl(Region_map &rm, Ram_allocator &ram,
			               bool executable = false)
			:
				_ds_pool(&ram, &rm, executable),
				_alloc(0),
				_chunk_size(MIN_CHUNK_SIZE)
			{ }

			void *alloc(size_t size, size_t align_log2);
			void free(void *ptr);
			size_t size_at(void const *ptr) const;
	};
}

#endif /* _LIBC__INTERNAL__MEM_ALLOC_H_ */
