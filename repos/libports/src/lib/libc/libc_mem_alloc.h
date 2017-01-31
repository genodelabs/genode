/*
 * \brief  Allocator for anonymous memory used by libc
 * \author Norman Feske
 * \date   2012-05-18
 */

#ifndef _LIBC_MEM_ALLOC_H_
#define _LIBC_MEM_ALLOC_H_

/* Genode includes */
#include <base/allocator.h>
#include <base/allocator_avl.h>
#include <base/env.h>
#include <util/list.h>
#include <ram_session/ram_session.h>
#include <rm_session/rm_session.h>

namespace Libc {

	struct Mem_alloc
	{
		virtual void *alloc(Genode::size_t size, Genode::size_t align_log2) = 0;
		virtual void free(void *ptr) = 0;
		virtual Genode::size_t size_at(void const *ptr) const = 0;
	};

	/**
	 * Return singleton instance of the memory allocator
	 */
	Mem_alloc *mem_alloc();

	class Mem_alloc_impl : public Mem_alloc
	{
		private:

			enum {
				MIN_CHUNK_SIZE =    4*1024,  /* in machine words */
				MAX_CHUNK_SIZE = 1024*1024
			};

			class Dataspace : public Genode::List<Dataspace>::Element
			{
				public:

					Genode::Ram_dataspace_capability cap;
					void  *local_addr;

					Dataspace(Genode::Ram_dataspace_capability c, void *a)
					: cap(c), local_addr(a) {}

					inline void * operator new(__SIZE_TYPE__, void* addr) {
						return addr; }

					inline void operator delete(void*) { }
			};

			class Dataspace_pool : public Genode::List<Dataspace>
			{
				private:

					Genode::Ram_session *_ram_session;  /* ram session for backing store */
					Genode::Region_map  *_region_map;   /* region map of address space   */

				public:

					/**
					 * Constructor
					 */
					Dataspace_pool(Genode::Ram_session *ram, Genode::Region_map *rm):
						_ram_session(ram), _region_map(rm) { }

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
					int expand(Genode::size_t size, Genode::Range_allocator *alloc);

					void reassign_resources(Genode::Ram_session *ram, Genode::Region_map *rm) {
						_ram_session = ram, _region_map = rm; }
			};

			Genode::Lock   mutable _lock;
			Dataspace_pool         _ds_pool;      /* list of dataspaces */
			Genode::Allocator_avl  _alloc;        /* local allocator    */
			Genode::size_t         _chunk_size;

			/**
			 * Try to allocate block at our local allocator
			 *
			 * \return true on success
			 *
			 * This function is a utility used by 'alloc' to avoid
			 * code duplication.
			 */
			bool _try_local_alloc(Genode::size_t size, void **out_addr);

		public:

			Mem_alloc_impl(Genode::Region_map &rm, Genode::Ram_session &ram)
			:
				_ds_pool(&ram, &rm),
				_alloc(0),
				_chunk_size(MIN_CHUNK_SIZE)
			{ }

			void *alloc(Genode::size_t size, Genode::size_t align_log2);
			void free(void *ptr);
			Genode::size_t size_at(void const *ptr) const;
	};
}

#endif /* _LIBC_MEM_ALLOC_H_ */
