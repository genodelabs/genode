/*
 * \brief  VirtualBox runtime (RT)
 * \author Norman Feske
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/allocator_avl.h>
#include <util/bit_allocator.h>


/* VirtualBox includes */
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include <iprt/semaphore.h>
#include <iprt/time.h>
#include <internal/iprt.h>

#include "mm.h"
#include "vmm.h"

enum {
	MEMORY_MAX = 128 * 1024 * 1024,
	MEMORY_CACHED = 16 * 1024 * 1024,
};

class Avl_ds : public Genode::Avl_node<Avl_ds>
{
	private:

		Genode::Ram_dataspace_capability _ds;
		Genode::addr_t _virt;
		Genode::addr_t const _size;
		Genode::addr_t _used_size;

		bool _inuse = true;

		static Genode::addr_t _mem_allocated;
		static Genode::addr_t _mem_unused;

		static Genode::Avl_tree<Avl_ds> _unused_ds;
		static Genode::Avl_tree<Avl_ds> _runtime_ds;

	public:

		static Genode::addr_t hit;
		static Genode::addr_t hit_coarse;

		Avl_ds(Genode::Ram_dataspace_capability ds, void * virt,
		       Genode::addr_t size)
		:
			_ds(ds), _virt(reinterpret_cast<Genode::addr_t>(virt)),
			_size(size), _used_size(_size)
		{
			_mem_allocated += _size;
			_runtime_ds.insert(this);
		}

		~Avl_ds() {
			Assert (!_inuse);

			_unused_ds.remove(this);
			_mem_unused -= _size;
			_mem_allocated -= _size;

			genode_env().ram().free(_ds);
			Genode::log("free up ", _size, " ", _mem_allocated, "/",
			            _mem_unused, " hit=", hit, "/", hit_coarse, " avail=",
			            genode_env().pd().avail_ram());
		}

		void unused()
		{
			_inuse = false;

			_runtime_ds.remove(this);

			_mem_unused += _size;
			_unused_ds.insert(this);
		}

		void used(Genode::addr_t size)
		{
			_inuse = true;

			_unused_ds.remove(this);
			_used_size = size;
			_mem_unused -= _size;
			_runtime_ds.insert(this);
		}

		bool higher(Avl_ds *e) {
				return _inuse ? e->_virt > _virt : e->_size > _size; }

		Avl_ds *find_virt(Genode::addr_t virt)
		{
			if (virt == _virt) return this;
			Avl_ds *obj = this->child(virt > _virt);
			return obj ? obj->find_virt(virt) : 0;
		}

		Avl_ds *find_coarse_match(Genode::addr_t size_min, Genode::addr_t size_max)
		{
			if (size_min <= _size && _size <= size_max) return this;
			Avl_ds *obj = this->child(size_max > _size);
			return obj ? obj->find_coarse_match(size_min, size_max) : 0;
		}

		Avl_ds *find_size(Genode::addr_t size, bool equal = true)
		{
			if (equal ? size == _size : size <= _size) return this;
			Avl_ds *obj = this->child(size > _size);
			return obj ? obj->find_size(size, equal) : 0;
		}

		static Avl_ds *find_match(Genode::addr_t size, bool coarse = false)
		{
			Avl_ds * head = _unused_ds.first();
			if (!head)
				return head;

			return coarse ? head->find_coarse_match(size, size * 2)
			              : head->find_size(size);
		}

		static Genode::addr_t max_size_at(void * p)
		{
			Avl_ds * ds_obj = Avl_ds::_runtime_ds.first();
			if (ds_obj)
				ds_obj = ds_obj->find_virt(reinterpret_cast<Genode::addr_t>(p));

			if (!ds_obj)
				return 0;

			return ds_obj->_size;
		}

		Genode::addr_t ds_virt() const { return _virt; }

		static void memory_freeup(Genode::addr_t const cb)
		{
			::size_t cbx = cb * 4;
			while (_unused_ds.first() && cbx &&
			       (_mem_allocated + cb > MEMORY_MAX ||
			        _mem_unused + cb > MEMORY_CACHED ||
			        genode_env().pd().avail_ram().value < cb * 2
			       )
			      )
			{
				Avl_ds * ds_free = _unused_ds.first()->find_size(cbx, false);
				if (!ds_free) {
					cbx = cbx / 2;
					continue;
				}

				destroy(vmm_heap(), ds_free);
			}
		}

		static void free_memory(void * pv, size_t cb)
		{
			if (cb % 0x1000)
				cb = (cb & ~0xFFFUL) + 0x1000UL;

			Avl_ds * ds_obj = Avl_ds::_runtime_ds.first();
			if (ds_obj)
				ds_obj = ds_obj->find_virt(reinterpret_cast<Genode::addr_t>(pv));

			if (ds_obj && ds_obj->_used_size == cb)
				ds_obj->unused();
			else {
				Genode::error(__func__, " unknown memory region ", pv, "(",
				              Genode::Hex(ds_obj ? ds_obj->ds_virt() : 0),
				              ")+", Genode::Hex(cb), "(",
				              Genode::Hex(ds_obj ? ds_obj->_size : 0), ")");
			}
		}
};

Genode::Avl_tree<Avl_ds> Avl_ds::_runtime_ds;
Genode::Avl_tree<Avl_ds> Avl_ds::_unused_ds;
static Genode::Lock lock_ds;

Genode::addr_t Avl_ds::hit = 0;
Genode::addr_t Avl_ds::hit_coarse = 0;
Genode::addr_t Avl_ds::_mem_allocated = 0;
Genode::addr_t Avl_ds::_mem_unused = 0;

static void *alloc_mem(size_t cb, const char *pszTag, bool executable = false)
{
	/* using managed dataspace to have all addresses within a 1 << 31 bit range */
	static Sub_rm_connection rt_memory(genode_env(), 2 * MEMORY_MAX);

	using namespace Genode;

	if (!cb)
		return nullptr;

	if (cb % 0x1000)
		cb = (cb & ~0xFFFUL) + 0x1000UL;

	Lock::Guard guard(lock_ds);

	if (Avl_ds * ds_free = Avl_ds::find_match(cb)) {
		ds_free->used(cb);

		Avl_ds::hit++;
		return reinterpret_cast<void *>(ds_free->ds_virt());
	} else
	if (Avl_ds * ds_free = Avl_ds::find_match(cb, true)) {
		ds_free->used(cb);

		Avl_ds::hit_coarse++;
		return reinterpret_cast<void *>(ds_free->ds_virt());
	}

	/* check for memory freeup, give hint about required memory (cb) */
	Avl_ds::memory_freeup(cb);

	try {
		Ram_dataspace_capability ds = genode_env().ram().alloc(cb);
		Assert(ds.valid());

		Genode::size_t const whole_size = 0;
		Genode::off_t  const offset     = 0;
		bool           const any_addr   = false;
		void *               any_local_addr = nullptr;

		void * local_addr = rt_memory.attach(ds, whole_size, offset,
		                                     any_addr, any_local_addr,
		                                     executable);

		Assert(local_addr);

		new (vmm_heap()) Avl_ds(ds, local_addr, cb);

		return local_addr;
	} catch (...) {
		Genode::error("Could not allocate RTMem* memory of size=", cb);
		return nullptr;
	}
}

/*
 * Called by the recompiler to allocate executable RAM
 */
void *RTMemExecAllocTag(size_t cb, const char *pszTag)
{
	return alloc_mem(cb, pszTag, true);
}


void *RTMemPageAllocZTag(size_t cb, const char *pszTag)
{
	/*
	 * The RAM dataspace freshly allocated by 'RTMemExecAllocTag' is zeroed
	 * already.
	 */
	void * addr = alloc_mem(cb, pszTag);
	if (addr)
		Genode::memset(addr, 0, cb);
	return addr;
}


void *RTMemPageAllocTag(size_t cb, const char *pszTag)
{
	return alloc_mem(cb, pszTag);
}


void RTMemPageFree(void *pv, size_t cb)
{
	Genode::Lock::Guard guard(lock_ds);

	Avl_ds::free_memory(pv, cb);
}

/**
 * The tiny code generator (TCG) of the REM allocates quite a hugh amount
 * of individal TCG_CACHED_SIZE blocks. Using a dataspace per allocation
 * increases the cap count significantly (e.g. 9G RAM caused 2500 allocations).
 * Using a Slab for the known size avoids the cap issue.
 */

enum { TCG_CACHE = 4 * 1024 * 1024, TCG_CACHED_SIZE = 0x4000 };

typedef Genode::Bit_allocator<TCG_CACHE / TCG_CACHED_SIZE> Tcg_idx_allocator;

struct Tcg_slab : Genode::List<Tcg_slab>::Element {
	Tcg_idx_allocator _slots;
	Genode::addr_t    _base { 0 };
	bool              _full { false };

	Tcg_slab(void *memory)
	: _base(reinterpret_cast<Genode::addr_t>(memory)) { };

	void * alloc()
	{
		unsigned idx = _slots.alloc();
		return reinterpret_cast<void *>(_base + idx * TCG_CACHED_SIZE);
	}

	Genode::addr_t contains(Genode::addr_t const ptr) const {
		return _base <= ptr && ptr < _base + TCG_CACHED_SIZE; }
};

static Genode::List<Tcg_slab> list;

void * RTMemTCGAlloc(size_t cb)
{
	if (cb != TCG_CACHED_SIZE)
		return alloc_mem(cb, __func__);

	{
		Genode::Lock::Guard guard(lock_ds);

		for (Tcg_slab * tcg = list.first(); tcg; tcg = tcg->next()) {
			if (tcg->_full)
				continue;

			try {
				return tcg->alloc();
			} catch (Tcg_idx_allocator::Out_of_indices) {
				tcg->_full = true;
				/* try on another slab */
			}
		}
	}

	Tcg_slab * tcg = new (vmm_heap()) Tcg_slab(alloc_mem(TCG_CACHE, __func__));
	if (tcg && tcg->_base) {
		{
			Genode::Lock::Guard guard(lock_ds);
			list.insert(tcg);
		}
		return RTMemTCGAlloc(cb);
	}

	Genode::error("no memory left for TCG");

	if (tcg)
		destroy(vmm_heap(), tcg);

	return nullptr;
}


void * RTMemTCGAllocZ(size_t cb)
{
	void * ptr = RTMemTCGAlloc(cb);
	if (ptr)
		Genode::memset(ptr, 0, cb);

	return ptr;
}

void RTMemTCGFree(void *pv)
{
	Genode::Lock::Guard guard(lock_ds);

	Genode::addr_t const ptr = reinterpret_cast<Genode::addr_t>(pv);
	for (Tcg_slab * tcg = list.first(); tcg; tcg = tcg->next()) {
		if (!tcg->contains(ptr))
			continue;

		Genode::warning("could not free up TCG memory ", pv);
		return;
	}
	Avl_ds::free_memory(pv, Avl_ds::max_size_at(pv));
}

void * RTMemTCGRealloc(void *ptr, size_t size)
{
	if (!ptr && size)
		return RTMemTCGAllocZ(size);

	if (!size) {
		if (ptr)
			RTMemTCGFree(ptr);
		return nullptr;
	}

	Genode::addr_t max_size = 0;
	{
		Genode::Lock::Guard guard(lock_ds);

		max_size = Avl_ds::max_size_at(ptr);
		if (!max_size) {
			Genode::error("bug - unknown pointer");
			return nullptr;
		}

		if (size <= max_size)
			return ptr;
	}

	void * new_ptr = RTMemTCGAllocZ(size);
	if (!new_ptr) {
		Genode::error("no memory left ", size);
		return nullptr;
	}
	Genode::memcpy(new_ptr, ptr, max_size);

	RTMemTCGFree(ptr);

	return new_ptr;
}

#include <iprt/buildconfig.h>

uint32_t RTBldCfgVersionMajor(void) { return VBOX_VERSION_MAJOR; }
uint32_t RTBldCfgVersionMinor(void) { return VBOX_VERSION_MINOR; }
uint32_t RTBldCfgVersionBuild(void) { return VBOX_VERSION_BUILD; }
uint32_t RTBldCfgRevision(void)     { return ~0; }


extern "C" DECLHIDDEN(int) rtProcInitExePath(char *pszPath, size_t cchPath)
{
	Genode::strncpy(pszPath, "/virtualbox", cchPath);
	return 0;
}
