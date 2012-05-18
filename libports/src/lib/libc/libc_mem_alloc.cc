/*
 * \brief  Allocator for anonymous memory used by libc
 * \author Norman Feske
 * \date   2012-05-18
 *
 * The libc uses a dedicated allocator instead of 'env()->heap()' because the
 * 'Allocator' interface of 'env()->heap()' does not allow for aligned
 * allocations. Some libc functions, however, rely on aligned memory. For
 * example the blocks returned by mmap for allocating anonymous memory are
 * assumed to be page-aligned.
 *
 * The code is largely based on 'base/include/base/heap.h' and
 * 'base/src/base/heap/heap.cc'.
 */

/* Genode includes */
#include <base/env.h>
#include <base/allocator_avl.h>
#include <base/printf.h>

/* local includes */
#include "libc_mem_alloc.h"

using namespace Genode;


namespace Libc {

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

			Lock           _lock;
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

			Mem_alloc_impl()
			:
				_ds_pool(env()->ram_session(), env()->rm_session()),
				_alloc(0),
				_chunk_size(MIN_CHUNK_SIZE)
			{ }

			void *alloc(Genode::size_t size, Genode::size_t align_log2);
			void free(void *ptr);
	};
}


using namespace Genode;;


Libc::Mem_alloc_impl::Dataspace_pool::~Dataspace_pool()
{
	/* free all ram_dataspaces */
	for (Dataspace *ds; (ds = first()); ) {

		/*
		 * read dataspace capability and modify _ds_list before detaching
		 * possible backing store for Dataspace - we rely on LIFO list
		 * manipulation here!
		 */

		Ram_dataspace_capability ds_cap = ds->cap;

		remove(ds);
		delete ds;
		_rm_session->detach(ds->local_addr);
		_ram_session->free(ds_cap);
	}
}


int Libc::Mem_alloc_impl::Dataspace_pool::expand(size_t size, Range_allocator *alloc)
{
	Ram_dataspace_capability new_ds_cap;
	void *local_addr, *ds_addr = 0;

	/* make new ram dataspace available at our local address space */
	try {
		new_ds_cap = _ram_session->alloc(size);
		local_addr = _rm_session->attach(new_ds_cap);
	} catch (Ram_session::Alloc_failed) {
		return -2;
	} catch (Rm_session::Attach_failed) {
		_ram_session->free(new_ds_cap);
		return -3;
	}

	/* add new local address range to our local allocator */
	alloc->add_range((addr_t)local_addr, size);

	/* now that we have new backing store, allocate Dataspace structure */
	if (!alloc->alloc_aligned(sizeof(Dataspace), &ds_addr, 2)) {
		PWRN("could not allocate meta data - this should never happen");
		return -1;
	}

	/* add dataspace information to list of dataspaces */
	Dataspace *ds  = new (ds_addr) Dataspace(new_ds_cap, local_addr);
	insert(ds);

	return 0;
}


void *Libc::Mem_alloc_impl::alloc(size_t size, size_t align_log2)
{
	/* serialize access of heap functions */
	Lock::Guard lock_guard(_lock);

	/* try allocation at our local allocator */
	void *out_addr = 0;
	if (_alloc.alloc_aligned(size, &out_addr, align_log2))
		return out_addr;

	/*
	 * Calculate block size of needed backing store. The block must hold the
	 * requested 'size' and a new Dataspace structure if the allocation above
	 * failed. Finally, we align the size to a 4K page.
	 */
	size_t request_size = size + 1024;

	if (request_size < _chunk_size*sizeof(umword_t)) {
		request_size = _chunk_size*sizeof(umword_t);

		/*
		 * Exponentially increase chunk size with each allocated chunk until
		 * we hit 'MAX_CHUNK_SIZE'.
		 */
		_chunk_size = min(2*_chunk_size, (size_t)MAX_CHUNK_SIZE);
	}

	if (_ds_pool.expand(align_addr(request_size, 12), &_alloc) < 0) {
		PWRN("could not expand dataspace pool");
		return 0;
	}

	/* allocate originally requested block */
	return _alloc.alloc_aligned(size, &out_addr, align_log2) ? out_addr : 0;
}


void Libc::Mem_alloc_impl::free(void *addr)
{
	/* serialize access of heap functions */
	Lock::Guard lock_guard(_lock);

	/* forward request to our local allocator */
	_alloc.free(addr);
}


Libc::Mem_alloc *Libc::mem_alloc()
{
	static Libc::Mem_alloc_impl inst;
	return &inst;
}


