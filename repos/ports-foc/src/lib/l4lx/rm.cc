/*
 * \brief  Region map for l4lx support library.
 * \author Stefan Kalkowski
 * \date   2011-04-11
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <rm_session/connection.h>
#include <base/capability.h>
#include <util/misc_math.h>

/* L4lx includes */
#include <env.h>

namespace Fiasco {
#include <l4/sys/types.h>
#include <l4/sys/task.h>
#include <l4/util/util.h>
}

using namespace L4lx;

Region* Region_manager::find_region(Genode::addr_t *addr, Genode::size_t *size)
{
	Genode::Allocator_avl_base::Block *b = _find_by_address(*addr);
	if (!b)
		return 0;

	*addr = b->addr();
	*size = b->size();
	return b->used() ? metadata((void*)*addr) : 0;
}


void* Region_manager::attach(Genode::Dataspace_capability cap, const char* name)
{
	/* Put it in the dataspace tree */
	L4lx::Dataspace *ds =
		L4lx::Env::env()->dataspaces()->insert(name, cap);

	return attach(ds);
}


void* Region_manager::attach(Dataspace *ds)
{
	void* addr = Genode::env()->rm_session()->attach(ds->cap());
	alloc_addr(ds->size(), (Genode::addr_t)addr);
	metadata(addr, Region((Genode::addr_t)addr, ds->size(), ds));
	return addr;
}


bool Region_manager::attach_at(Dataspace *ds, Genode::size_t size,
                               Genode::size_t offset, void* addr)
{
	Genode::Allocator_avl_base::Block *b = _find_by_address((Genode::addr_t)addr);

	/* If the region is already known, it should have been reserved before */
	if (b && b->used()) {
		Region *r = metadata(addr);

		/* Sanity check */
		if (!r || r->addr() != (Genode::addr_t)addr ||
		    r->size() != ds->size() || r->ds()) {
			return false;
		}

		/* We have to detach the dataspace placeholder */
		Genode::env()->rm_session()->detach(addr);
	} else
		/* We have to reserve the area in our region map */
		alloc_addr(ds->size(), (Genode::addr_t)addr);

	/* Now call Genode's region map to really attach the dataspace */
	try {
		Genode::env()->rm_session()->attach(ds->cap(), size, offset,
		                                    true, (Genode::addr_t) addr);
	} catch(...) {
		return false;
	}
	metadata(addr, Region((Genode::addr_t)addr, ds->size(), ds));
	return true;
}


Region* Region_manager::reserve_range(Genode::size_t size, int align,
                                      Genode::addr_t start)
{
	using namespace Genode;
	void* addr = nullptr;
	addr_t original_start = start;

	while (true) {

		Rm_connection *rmc = nullptr;
		Region_map    *rm  = nullptr;

		try {
			/*
			 * We attach a managed-dataspace as a placeholder to
			 * Genode's region-map
			 */
			rmc = new (env()->heap()) Rm_connection;
			rm  = new (env()->heap()) Region_map_client(rmc->create(size));

			addr = start ? env()->rm_session()->attach_at(rm->dataspace(), start)
			             : env()->rm_session()->attach(rm->dataspace());
			break;
		} catch(Rm_session::Attach_failed e) {
			destroy(env()->heap(), rm);
			 /* attach with pre-defined address failed, so search one */
			if (start) {
				/* the original start address might have a different alignment */
				addr_t aligned_start = align_addr(start, align);
				if (aligned_start != start) {
					start = aligned_start;
				} else {
					if (start <= ((addr_t)~0 - 2*(1 << align) + 1)) {
						start += (1 << align);
					} else {
						warning(__func__, ": attach failed: "
						        "start=", Hex(original_start), " "
						        "size=",  Hex(size),           " "
						        "align=", align);
						return 0;
					}
				}
			} else {
				warning(__func__, ": attach failed: "
				        "start=", Hex(original_start), " "
				        "size=",  Hex(size),           " "
				        "align=", align);
				return 0;
			}
		}
	}

	/*
	 * Mark the region reserved, in our region-map by setting the
	 * dataspace reference to zero.
	 */
	alloc_addr(size, (addr_t)addr);
	Region reg((addr_t)addr, size, 0);
	metadata(addr, reg);
	return metadata(addr);
}


void Region_manager::reserve_range(Genode::addr_t addr, Genode::size_t size,
                                   const char *name)
{
	Genode::Dataspace_capability cap;
	L4lx::Dataspace *ds = new (Genode::env()->heap())
		L4lx::Single_dataspace(name, size, cap);
	L4lx::Env::env()->dataspaces()->insert(ds);
	alloc_addr(size, (Genode::addr_t)addr);
	metadata((void*)addr, Region(addr, size, ds));
}


void Region_manager::dump()
{
	Genode::addr_t addr = 0;
	Genode::log("Region map:");
	while (true) {
		Allocator_avl_base::Block *b = _find_by_address(addr);
		Region *r = metadata((void*)addr);
		if (!b)
			return;

		Genode::log("     ", Genode::Hex_range<Genode::addr_t>(b->addr(), b->size()),
		            " [", b->used() ? ((r && r->ds()) ? r->ds()->name() : "reserved")
		                            : "unused",
		            "]");

		addr = b->addr() + b->size();
	}
};


Mapping* Region_manager::_virt_to_phys(void *virt) {
	return _virt_tree.first() ? _virt_tree.first()->find_by_virt(virt) : 0; }


Phys_mapping* Region_manager::_phys_to_virt(void *phys) {
	return _phys_tree.first() ? _phys_tree.first()->find_by_phys(phys) : 0; }


void Region_manager::add_mapping(void *phys, void *virt, bool rw)
{
	Mapping *m = _virt_to_phys(virt);
	if (!m) {
		m = new (Genode::env()->heap()) Mapping(virt, phys, rw);
		_virt_tree.insert(m);
		Phys_mapping *p = _phys_to_virt(phys);
		if (!p) {
			p = new (Genode::env()->heap()) Phys_mapping(phys);
			_phys_tree.insert(p);
		}
		p->mappings()->insert(m);
	}
}


void Region_manager::remove_mapping(void *virt)
{
	using namespace Fiasco;

	l4_fpage_t fpage = l4_fpage((l4_addr_t)virt, L4_LOG2_PAGESIZE, L4_FPAGE_RW);
	l4_msgtag_t tag  = l4_task_unmap(L4_BASE_TASK_CAP, fpage, L4_FP_ALL_SPACES);
	if (l4_error(tag))
		Genode::warning("unmapping ", virt, " failed, error=", l4_error(tag));

	Mapping *m = _virt_to_phys(virt);
	if (m) {
		_virt_tree.remove(m);
		Phys_mapping *p = _phys_to_virt(m->phys());
		if (p) {
			p->mappings()->remove(m);
			if (!p->mappings()->first()) {
				_phys_tree.remove(p);
				Genode::destroy(Genode::env()->heap(), p);
			}
		}
		Genode::destroy(Genode::env()->heap(), m);
	}
}


void Region_manager::map(void *phys)
{
	using namespace Fiasco;

	Phys_mapping *p = _phys_to_virt(phys);
	if (p) {
		Mapping *m = p->mappings()->first();
		while (m) {
			if (!m->writeable())
				l4_touch_ro(phys, L4_PAGESIZE);
			else
				l4_touch_rw(phys, L4_PAGESIZE);

			l4_fpage_t snd_fpage = m->writeable()
				? l4_fpage((l4_addr_t)phys, L4_LOG2_PAGESIZE, L4_FPAGE_RW)
				: l4_fpage((l4_addr_t)phys, L4_LOG2_PAGESIZE, L4_FPAGE_RO);
			l4_msgtag_t tag = l4_task_map(L4_BASE_TASK_CAP, L4_BASE_TASK_CAP,
			                              snd_fpage, (l4_addr_t)m->virt());
			if (l4_error(tag)) {
				Genode::error("mapping from ", phys, " to ", m->virt(), " "
				              "failed, error=", l4_error(tag));
			}
			m = m->next();
		}
	}
}


void* Region_manager::phys(void *virt)
{
	Mapping *m = _virt_to_phys(virt);
	return m ? m->phys() : 0;
}

