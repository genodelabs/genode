/*
 * \brief  VirtualBox runtime (RT)
 * \author Norman Feske
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013-2016 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/env.h>
#include <base/allocator_avl.h>


/* VirtualBox includes */
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include <iprt/semaphore.h>
#include <iprt/time.h>
#include <internal/iprt.h>

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

			Genode::env()->ram_session()->free(_ds);
			Genode::log("free up ", _size, " ", _mem_allocated, "/",
			            _mem_unused, " hit=", hit, "/", hit_coarse, " avail=",
			            Genode::env()->ram_session()->avail());
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

		Genode::addr_t ds_virt() const { return _virt; }

		static void memory_freeup(Genode::addr_t const cb)
		{
			/* free up memory if we hit some chosen limits */
			enum {
				MEMORY_MAX = 64 * 1024 * 1024,
				MEMORY_CACHED = 16 * 1024 * 1024,
			};

			size_t cbx = cb * 4;
			while (_unused_ds.first() && cbx &&
			       (_mem_allocated + cb > MEMORY_MAX ||
			        _mem_unused + cb > MEMORY_CACHED ||
			        Genode::env()->ram_session()->avail() < cb * 2
			       )
			      )
			{
				Avl_ds * ds_free = _unused_ds.first()->find_size(cbx, false);
				if (!ds_free) {
					cbx = cbx / 2;
					continue;
				}

				destroy(Genode::env()->heap(), ds_free);
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
		Ram_dataspace_capability ds = env()->ram_session()->alloc(cb);
		Assert(ds.valid());

		size_t        const whole_size = 0;
		Genode::off_t const offset     = 0;
		bool          const any_addr   = false;
		void *              any_local_addr = nullptr;

		void * local_addr = env()->rm_session()->attach(ds, whole_size, offset,
		                                                any_addr, any_local_addr,
		                                                executable);

		Assert(local_addr);

		new (env()->heap()) Avl_ds(ds, local_addr, cb);

		return local_addr;
	} catch (...) {
		Genode::error("Could not allocate RTMem* memory of size=", cb);
		return nullptr;
	}
}

#ifndef RT_NO_THROW
	/* not defined in vbox5, but this code is used by vbox4 and vbox5 */
	#define RT_NO_THROW
#endif

/*
 * Called by the recompiler to allocate executable RAM
 */
void *RTMemExecAllocTag(size_t cb, const char *pszTag) RT_NO_THROW
{
	return alloc_mem(cb, pszTag, true);
}


void *RTMemPageAllocZTag(size_t cb, const char *pszTag) RT_NO_THROW
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


void *RTMemPageAllocTag(size_t cb, const char *pszTag) RT_NO_THROW
{
	return alloc_mem(cb, pszTag);
}


void RTMemPageFree(void *pv, size_t cb) RT_NO_THROW
{
	Genode::Lock::Guard guard(lock_ds);

	Avl_ds::free_memory(pv, cb);
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
