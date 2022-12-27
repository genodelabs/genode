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
 * 'base/src/lib/base/heap.cc'.
 */

/* Genode includes */
#include <base/env.h>
#include <base/allocator_avl.h>

/* libc-internal includes */
#include <internal/mem_alloc.h>
#include <internal/init.h>

using namespace Genode;


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
		void const * const local_addr = ds->local_addr;

		remove(ds);
		delete ds;

		_region_map->detach(local_addr);
		_ram->free(ds_cap);
	}
}


int Libc::Mem_alloc_impl::Dataspace_pool::expand(size_t size, Range_allocator *alloc)
{
	Ram_dataspace_capability new_ds_cap;
	void *local_addr;

	/* make new ram dataspace available at our local address space */
	try {
		new_ds_cap = _ram->alloc(size);

		enum { MAX_SIZE = 0, NO_OFFSET = 0, ANY_LOCAL_ADDR = false };
		local_addr = _region_map->attach(new_ds_cap, MAX_SIZE, NO_OFFSET,
		                                 ANY_LOCAL_ADDR, nullptr, _executable);
	}
	catch (Out_of_ram) { return -2; }
	catch (Out_of_caps) { return -4; }
	catch (Region_map::Region_conflict) {
		_ram->free(new_ds_cap);
		return -3;
	}

	/* add new local address range to our local allocator */
	alloc->add_range((addr_t)local_addr, size);

	/* now that we have new backing store, allocate Dataspace structure */
	return alloc->alloc_aligned(sizeof(Dataspace), 2).convert<int>(

		[&] (void *ptr) {
			/* add dataspace information to list of dataspaces */
			Dataspace *ds  = construct_at<Dataspace>(ptr, new_ds_cap, local_addr);
			insert(ds);
			return 0; },

		[&] (Allocator::Alloc_error) {
			warning("libc: could not allocate meta data - this should never happen");
			return -1; });
}


void *Libc::Mem_alloc_impl::alloc(size_t size, size_t align_log2)
{
	/* serialize access of heap functions */
	Mutex::Guard guard(_mutex);

	void *out_addr = nullptr;

	/* try allocation at our local allocator */
	_alloc.alloc_aligned(size, align_log2).with_result(
		[&] (void *ptr) { out_addr = ptr; },
		[&] (Allocator::Alloc_error) { });

	if (out_addr)
		return out_addr;

	/*
	 * Calculate block size of needed backing store. The block must hold the
	 * requested 'size' with the requested alignment, a new Dataspace structure
	 * and space for AVL-node slab blocks if the allocation above failed.
	 * Finally, we align the size to a 4K page.
	 */
	size_t request_size = size + max((1 << align_log2), 1024) +
	                      Allocator_avl::slab_block_size() + sizeof(Dataspace);

	if (request_size < _chunk_size*sizeof(umword_t)) {
		request_size = _chunk_size*sizeof(umword_t);

		/*
		 * Exponentially increase chunk size with each allocated chunk until
		 * we hit 'MAX_CHUNK_SIZE'.
		 */
		_chunk_size = min(2*_chunk_size, (size_t)MAX_CHUNK_SIZE);
	}

	if (_ds_pool.expand(align_addr(request_size, 12), &_alloc) < 0) {
		warning("libc: could not expand dataspace pool");
		return 0;
	}

	/* allocate originally requested block */
	_alloc.alloc_aligned(size, align_log2).with_result(
		[&] (void *ptr) { out_addr = ptr; },
		[&] (Allocator::Alloc_error) { });

	return out_addr;
}


void Libc::Mem_alloc_impl::free(void *addr)
{
	/* serialize access of heap functions */
	Mutex::Guard guard(_mutex);

	/* forward request to our local allocator */
	_alloc.free(addr);
}


Libc::Mem_alloc::Size_at_result Libc::Mem_alloc_impl::size_at(void const *addr) const
{
	/* serialize access of heap functions */
	Mutex::Guard guard(_mutex);

	/* forward request to our local allocator */
	return _alloc.size_at(addr);
}


static Libc::Mem_alloc *_libc_mem_alloc_rw  = nullptr;
static Libc::Mem_alloc *_libc_mem_alloc_rwx = nullptr;


static void _init_mem_alloc(Region_map &rm, Ram_allocator &ram)
{
	enum { MEMORY_EXECUTABLE = true };

	static Libc::Mem_alloc_impl inst_rw(rm, ram, !MEMORY_EXECUTABLE);
	static Libc::Mem_alloc_impl inst_rwx(rm, ram, MEMORY_EXECUTABLE);

	_libc_mem_alloc_rw  = &inst_rw;
	_libc_mem_alloc_rwx = &inst_rwx;
}


void Libc::init_mem_alloc(Genode::Env &env)
{
	_init_mem_alloc(env.rm(), env.ram());
}


Libc::Mem_alloc *Libc::mem_alloc(bool executable)
{
	if (!_libc_mem_alloc_rw || !_libc_mem_alloc_rwx)
		error("attempt to use 'Libc::mem_alloc' before call of 'init_mem_alloc'");

	return executable ? _libc_mem_alloc_rwx : _libc_mem_alloc_rw;
}
