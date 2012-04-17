/*
 * \brief  Implementation of the RM session interface
 * \author Christian Helmuth
 * \author Norman Feske
 * \date   2006-07-17
 *
 * FIXME arg_string and quota missing
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/lock.h>
#include <util/arg_string.h>
#include <util/misc_math.h>

/* core includes */
#include <util.h>
#include <cpu_session_component.h>
#include <rm_session_component.h>
#include <dataspace_component.h>

using namespace Genode;


static const bool verbose             = false;
static const bool verbose_page_faults = false;


namespace Genode {

	struct Rm_session_component::Fault_area
	{
		addr_t _fault_addr;
		addr_t _base;
		size_t _size_log2;

		addr_t _upper_bound() const {
			return (_size_log2 == ~0UL) ? ~0 : (_base + (1 << _size_log2) - 1); }


		/**
		 * Default constructor, constructs invalid fault area
		 */
		Fault_area() : _size_log2(0) { }

		/**
		 * Constructor, fault area spans the maximum address-space size
		 */
		Fault_area(addr_t fault_addr) :
			_fault_addr(fault_addr), _base(0), _size_log2(~0) { }

		/**
		 * Constrain fault area to specified region
		 */
		void constrain(addr_t region_base, size_t region_size)
		{
			/*
			 * Find flexpage around _fault_addr that lies within the
			 * specified region.
			 *
			 * Start with a 'size_log2' of one less than the minimal
			 * page size. If the specified constraint conflicts with
			 * the existing fault area, the loop breaks at the first
			 * iteration and we can check for this condition after the
			 * loop.
			 */
			size_t size_log2 = get_page_size_log2() - 1;
			addr_t base = 0;
			for (size_t try_size_log2 = get_page_size_log2();
			     try_size_log2 < sizeof(addr_t)*8 ; try_size_log2++) {
				addr_t fpage_mask = ~((1 << try_size_log2) - 1);
				addr_t try_base = _fault_addr & fpage_mask;

				/* check lower bound of existing fault area */
				if (try_base < _base)
					break;

				/* check against upper bound of existing fault area */
				if (try_base + (1UL << try_size_log2) - 1 > _upper_bound())
					break;

				/* check against lower bound of region */
				if (try_base < region_base)
					break;

				/* check against upper bound of region */
				if (try_base + (1 << try_size_log2) - 1 > region_base + region_size - 1)
					break;

				/* flexpage is compatible with fault area, use it */
				size_log2 = try_size_log2;
				base      = try_base;
			}

			/* if constraint is compatible with the fault area, invalidate */
			if (size_log2 < get_page_size_log2()) {
				_size_log2 = 0;
				_base      = 0;
			} else {
				_size_log2 = size_log2;
				_base      = base;
			}
		}

		/**
		 * Constrain fault area to specified flexpage size
		 */
		void constrain(size_t size_log2)
		{
			if (size_log2 >= _size_log2)
				return;

			_base = _fault_addr & ~((1 << size_log2) - 1);
			_size_log2 = size_log2;
		}

		/**
		 * Determine common flexpage size compatible with specified fault areas
		 */
		static size_t common_size_log2(Fault_area const &a1, Fault_area const &a2)
		{
			/*
			 * We have to make sure that the offset of page-fault address
			 * relative to the flexpage base is the same for both fault areas.
			 * This condition is met by the flexpage size equal to the number
			 * of common least-significant bits of both offsets.
			 */
			size_t const diff = (a1.fault_addr() - a1.base())
			                  ^ (a2.fault_addr() - a2.base());

			/*
			 * Find highest clear bit in 'diff', starting from the least
			 * significant candidate. We can skip all bits lower then
			 * 'get_page_size_log2()' because they are not relevant as
			 * flexpage size (and are always zero).
			 */
			size_t n = get_page_size_log2();
			size_t const min_size_log2 = min(a1._size_log2, a2._size_log2);
			for (; n < min_size_log2 && !(diff & (1 << n)); n++);

			return n;
		}

		addr_t fault_addr() const { return _fault_addr; }
		addr_t base()       const { return _base; }
		bool   valid()      const { return _size_log2 > 0; }
	};
}


/***************************
 ** Region-manager client **
 ***************************/

/*
 * This code is executed by the page-fault handler thread.
 */

int Rm_client::pager(Ipc_pager &pager)
{
	Rm_session::Fault_type pf_type = pager.is_write_fault() ? Rm_session::WRITE_FAULT
	                                                        : Rm_session::READ_FAULT;
	addr_t pf_addr = pager.fault_addr();
	addr_t pf_ip   = pager.fault_ip();

	if (verbose_page_faults)
		print_page_fault("page fault", pf_addr, pf_ip, pf_type, badge());

	Rm_session_component            *curr_rm_session = member_rm_session();
	addr_t                           curr_rm_base = 0;
	Dataspace_component             *src_dataspace = 0;
	Rm_session_component::Fault_area src_fault_area;
	Rm_session_component::Fault_area dst_fault_area(pf_addr);
	bool lookup;

	/* traverse potentially nested dataspaces until we hit a leaf dataspace */
	unsigned level;
	enum { MAX_NESTING_LEVELS = 5 };
	for (level = 0; level < MAX_NESTING_LEVELS; level++) {

		lookup = curr_rm_session->reverse_lookup(curr_rm_base,
		                                        &dst_fault_area,
		                                        &src_dataspace,
		                                        &src_fault_area);
		if (!lookup)
			break;

		/* check if we need to traverse into a nested dataspace */
		Rm_session_component *sub_rm_session = src_dataspace->sub_rm_session();
		if (!sub_rm_session)
			break;

		/* set up next iteration */

		curr_rm_base = dst_fault_area.fault_addr()
		             - src_fault_area.fault_addr() + src_dataspace->map_src_addr();
		curr_rm_session = sub_rm_session;
	}

	if (level == MAX_NESTING_LEVELS) {
		PWRN("Too many nesting levels of managed dataspaces");
		return -1;
	}

	if (!lookup) {

		/*
		 * We found no attachment at the page-fault address and therefore have
		 * to reflect the page fault as region-manager fault. The signal
		 * handler is then expected to request the state of the region-manager
		 * session.
		 */

		/* print a warning if it's no managed-dataspace */
		if (curr_rm_session == member_rm_session())
			print_page_fault("no RM attachment", pf_addr, pf_ip, pf_type, badge());

		/* register fault at responsible region-manager session */
		curr_rm_session->fault(this, dst_fault_area.fault_addr() - curr_rm_base, pf_type);
		/* there is no attachment return an error condition */
		return 1;
	}

	/*
	 * Determine mapping size compatible with source and destination,
	 * and apply platform-specific constraint of mapping sizes.
	 */
	size_t map_size_log2 = dst_fault_area.common_size_log2(dst_fault_area,
	                                                       src_fault_area);
	map_size_log2 = constrain_map_size_log2(map_size_log2);

	src_fault_area.constrain(map_size_log2);
	dst_fault_area.constrain(map_size_log2);

	/*
	 * Check if dataspace is compatible with page-fault type
	 */
	if (pf_type == Rm_session::WRITE_FAULT && !src_dataspace->writable()) {

		/* attempted there is no attachment return an error condition */
		print_page_fault("attempted write at read-only memory",
		                 pf_addr, pf_ip, pf_type, badge());

		/* register fault at responsible region-manager session */
		curr_rm_session->fault(this, src_fault_area.fault_addr(), pf_type);
		return 2;
	}

	Mapping mapping(dst_fault_area.base(),
	                src_fault_area.base(),
	                src_dataspace->write_combined(),
	                map_size_log2,
	                src_dataspace->writable());

	/*
	 * On kernels with a mapping database, the 'dsc' dataspace is a leaf
	 * dataspace that corresponds to a virtual address range within core. To
	 * prepare the answer for the page fault, we make sure that this range is
	 * locally mapped in core. On platforms that support map operations of
	 * pages that are not locally mapped, the 'map_core_local' function may be
	 * empty.
	 */
	if (!src_dataspace->is_io_mem())
		mapping.prepare_map_operation();

	/* answer page fault with a flex-page mapping */
	pager.set_reply_mapping(mapping);
	return 0;
}


/*************
 ** Faulter **
 *************/

void Rm_faulter::fault(Rm_session_component *faulting_rm_session,
                       Rm_session::State     fault_state)
{
	Lock::Guard lock_guard(_lock);

	_faulting_rm_session = faulting_rm_session;
	_fault_state         = fault_state;
}


void Rm_faulter::dissolve_from_faulting_rm_session()
{
	Lock::Guard lock_guard(_lock);

	if (_faulting_rm_session)
		_faulting_rm_session->discard_faulter(this);

	_faulting_rm_session = 0;
}


void Rm_faulter::continue_after_resolved_fault()
{
	Lock::Guard lock_guard(_lock);

	_pager_object->wake_up();
	_faulting_rm_session = 0;
	_fault_state = Rm_session::State();
}


/**************************************
 ** Region-manager-session component **
 **************************************/

Rm_session::Local_addr
Rm_session_component::attach(Dataspace_capability ds_cap, size_t size,
                             off_t offset, bool use_local_addr,
                             Rm_session::Local_addr local_addr,
                             bool executable)
{
	/* serialize access */
	Lock::Guard lock_guard(_lock);

	/* offset must be positive and page-aligned */
	if (offset < 0
	 || align_addr(offset, get_page_size_log2()) != offset)
		throw Invalid_args();

	/* check dataspace validity */
	Dataspace_component *dsc = dynamic_cast<Dataspace_component *>
	                           (_ds_ep->obj_by_cap(ds_cap));
	if (!dsc) throw Invalid_dataspace();

	if (!size) {
		size = dsc->size() - offset;

		if (dsc->size() <= (size_t)offset) {
			PWRN("size is 0");
			throw Invalid_dataspace();
		}
	}

	/* work with page granularity */
	size = align_addr(size, get_page_size_log2());

	/* allocate region for attachment */
	void *r = 0;
	if (use_local_addr) {
		switch (_map.alloc_addr(size, local_addr)) {

		case Range_allocator::OUT_OF_METADATA:
			throw Out_of_metadata();

		case Range_allocator::RANGE_CONFLICT:
			throw Region_conflict();

		case Range_allocator::ALLOC_OK:
			r = local_addr;
			break;
		}
	} else {

		/*
		 * Find optimal alignment for new region. First try natural alignment.
		 * If that is not possible, try again with successively less alignment
		 * constraints.
		 */
		size_t align_log2 = log2(size);
		for (; align_log2 >= get_page_size_log2(); align_log2--) {

			/*
			 * Don't use an aligment higher than the alignment of the backing
			 * store. The backing store would constrain the mapping size
			 * anyway such that a higher alignment of the region is of no use.
			 */
			if (((dsc->map_src_addr() + offset) & ((1 << align_log2) - 1)) != 0)
				continue;

			/* try allocating the align region */
			if (_map.alloc_aligned(size, &r, align_log2))
				break;
		}

		if (align_log2 < get_page_size_log2()) {
			_map.free(r);
			throw Region_conflict();
		}
	}

	/* store attachment info in meta data */
	_map.metadata(r, Rm_region((addr_t)r, size, true, dsc, offset, this));
	Rm_region *region = _map.metadata(r);

	/* also update region list */
	Rm_region_ref *p;
	try { p = new(&_ref_slab) Rm_region_ref(region); }
	catch (Allocator::Out_of_memory) {
		_map.free(r);
		throw Out_of_metadata();
	}

	_regions.insert(p);

	/* inform dataspace about attachment */
	dsc->attached_to(region);

	if (verbose)
		PDBG("attach ds %p (a=%lx,s=%zx,o=%lx) @ [%lx,%lx)",
		     dsc, dsc->phys_addr(), dsc->size(), offset, (addr_t)r, (addr_t)r + size);

	/* check if attach operation resolves any faulting region-manager clients */
	for (Rm_faulter *faulter = _faulters.first(); faulter; ) {

		/* remeber next pointer before possibly removing current list element */
		Rm_faulter *next = faulter->next();

		if (faulter->fault_in_addr_range((addr_t)r, size)) {
			_faulters.remove(faulter);
			faulter->continue_after_resolved_fault();
		}

		faulter = next;
	}

	return r;
}


static void unmap_managed(Rm_session_component *session, Rm_region *region, int level)
{
	for (Rm_region *managed = session->dataspace_component()->regions()->first();
	     managed;
	     managed = managed->List<Rm_region>::Element::next()) {

		if (verbose)
			PDBG("(%d: %p) a=%lx,s=%lx,off=%lx ra=%lx,s=%lx,off=%lx sub-session %p",
			     level, session, managed->base(), (long)managed->size(), managed->offset(),
			     region->base(), (long)region->size(), region->offset(), managed->session());

		if (managed->base() - managed->offset() >= region->base() - region->offset()
		    && managed->base() - managed->offset() + managed->size()
		       <= region->base() - region->offset() + region->size())
			unmap_managed(managed->session(), managed, level + 1);

		/* Found a leaf node (here a leaf is an Rm_session whose dataspace has no regions) */
		if (!managed->session()->dataspace_component()->regions()->first())
			for (Rm_client *rc = managed->session()->clients()->first();
			     rc; rc = rc->List<Rm_client>::Element::next())
				rc->unmap(region->dataspace()->core_local_addr() + region->offset(),
				          managed->base() + region->base() - managed->offset(), region->size());
	}
}


void Rm_session_component::detach(Local_addr local_addr)
{
	/* serialize access */
	Lock::Guard lock_guard(_lock);

	/* read meta data for address */
	Rm_region *region = _map.metadata(local_addr);

	if (!region) {
		PDBG("no attachment at %p", (void *)local_addr);
		return;
	}

	Dataspace_component *dsc = region->dataspace();
	if (!dsc)
		PWRN("Rm_region of %p may be inconsistent!", this);

	if (verbose)
		PDBG("detach ds %p (a=%lx,s=%zx,o=%lx) at [%lx,%lx)",
		     dsc, dsc->phys_addr(), dsc->size(), region->offset(),
		     region->base(), region->base() + region->size());

	/* inform dataspace about detachment */
	dsc->detached_from(region);

	/*
	 * Deallocate region on platforms that support unmap
	 *
	 * On platforms without support for unmap (in particular NOVA 0.1), the
	 * same virtual address must not be reused. Hence, we never mark used
	 * regions as free.
	 *
	 * We unregister the region from region map prior unmapping the pages to
	 * make sure that page faults occurring immediately after the unmap
	 * refer to an empty region not to the dataspace, which we just removed.
	 */
	if (platform()->supports_unmap())
		_map.free(local_addr);

	/*
	 * This function gets called from the destructor of 'Dataspace_component',
	 * which iterates through all regions the dataspace is attached to. One
	 * particular case is the destruction of an 'Rm_session_component' and its
	 * contained managed dataspace ('_ds') member. The type of this member is
	 * derived from 'Dataspace_component' and provides the 'sub_rm_session'
	 * function, which can normally be used to distinguish managed dataspaces
	 * from leaf dataspaces. However, at destruction time of the '_dsc' base
	 * class, the vtable entry of 'sub_rm_session' already points to the
	 * base-class's function. Hence, we cannot check the return value of this
	 * function to determine if the dataspace is a managed dataspace. Instead,
	 * we introduced a dataspace member '_managed' with the non-virtual accessor
	 * function 'is_managed'.
	 */

	/*
	 * Go through all RM clients using the RM session. For each RM client, we
	 * need to unmap the referred region from its virtual address space.
	 */
	for (Rm_client *rc = _clients.first(); rc; rc = rc->List<Rm_client>::Element::next()) {

		/*
		 * XXX Unmapping managed dataspaces on kernels, which take a core-
		 *     local virtual address as unmap argument is not supported yet.
		 *     This is the case for Fiasco, Pistachio, and NOVA. On those
		 *     kernels, the unmap operation must be issued for each leaf
		 *     dataspace the managed dataspace is composed of. For kernels with
		 *     support for directed unmap (OKL4 or Codezero), unmap can be
		 *     simply applied for the contiguous virtual address region in the
		 *     client.
		 */
		if (!platform()->supports_direct_unmap()
		 && dsc->is_managed() && dsc->core_local_addr() == 0) {
			PWRN("unmapping of managed dataspaces not yet supported");
			break;
		}

		rc->unmap(dsc->core_local_addr() + region->offset(),
		          region->base(), region->size());
	}

	/*
	 * If RM session is used as nested dataspace, unmap this
	 * dataspace from all RM sessions.
	 */
	unmap_managed(this, region, 1);

	/* update region list */
	Rm_region_ref *p = _regions.first();
	for (; p; p = p->next())
		if (p->region() == region) break;

	if (p) {
		_regions.remove(p);
		destroy(&_ref_slab, p);
	}
}


Pager_capability Rm_session_component::add_client(Thread_capability thread)
{
	/* serialize access */
	Lock::Guard lock_guard(_lock);

	/* lookup thread and setup correct parameters */
	Cpu_thread_component *cpu_thread = dynamic_cast<Cpu_thread_component *>
	                                   (_thread_ep->obj_by_cap(thread));
	if (!cpu_thread) throw Invalid_thread();

	/* determine identification of client when faulting */
	unsigned long badge = cpu_thread->platform_thread()->pager_object_badge();

	Rm_client *cl;
	try { cl = new(&_client_slab) Rm_client(this, badge); }
	catch (Allocator::Out_of_memory) { throw Out_of_memory(); }

	_clients.insert(cl);

	return Pager_capability(_pager_ep->manage(cl));
}


bool Rm_session_component::reverse_lookup(addr_t                dst_base,
                                          Fault_area           *dst_fault_area,
                                          Dataspace_component **src_dataspace,
                                          Fault_area           *src_fault_area)
{
	/* serialize access */
	Lock::Guard lock_guard(_lock);

	/* rm-session-relative fault address */
	addr_t fault_addr = dst_fault_area->fault_addr() - dst_base;

	/* lookup region */
	Rm_region *region = _map.metadata((void*)fault_addr);
	if (!region)
		return false;

	/* request dataspace  backing the region */
	*src_dataspace = region->dataspace();
	if (!*src_dataspace)
		return false;

	/*
	 * Constrain destination fault area to region
	 *
	 * Handle corner case when the 'dst_base' is negative. In this case, we
	 * determine the largest flexpage within the positive portion of the
	 * region.
	 */
	addr_t region_base = region->base() + dst_base;
	size_t region_size = region->size();

	/* check for overflow condition */
	while ((long)region_base < 0 && (long)(region_base + region_size) > 0) {

		/* increment base address by half of the region size */
		region_base += region_size >> 1;

		/* lower the region size by one log2 step */
		region_size >>= 1;
	}
	dst_fault_area->constrain(region_base, region_size);

	/* calculate source fault address relative to 'src_dataspace' */
	addr_t src_fault_offset = fault_addr - region->base() + region->offset();

	addr_t src_base = (*src_dataspace)->map_src_addr();
	*src_fault_area = Fault_area(src_base + src_fault_offset);

	/* constrain source fault area by the source dataspace dimensions */
	src_fault_area->constrain(src_base, (*src_dataspace)->size());

	return src_fault_area->valid() && dst_fault_area->valid();
}


void Rm_session_component::fault(Rm_faulter *faulter, addr_t pf_addr,
                                 Rm_session::Fault_type pf_type)
{

	/* serialize access */
	Lock::Guard lock_guard(_lock);

	/* remeber fault state in faulting thread */
	faulter->fault(this, Rm_session::State(pf_type, pf_addr));

	/* enqueue faulter */
	_faulters.insert(faulter);

	/* issue fault signal */
	_fault_notifier.submit();
}


void Rm_session_component::discard_faulter(Rm_faulter *faulter)
{
	/* serialize access */
	Lock::Guard lock_guard(_lock);

	_faulters.remove(faulter);
}


void Rm_session_component::fault_handler(Signal_context_capability handler)
{
	_fault_notifier.context(handler);
}


Rm_session::State Rm_session_component::state()
{
	/* serialize access */
	Lock::Guard lock_guard(_lock);

	/* pick one of the currently faulted threads */
	Rm_faulter *faulter = _faulters.first();

	/* return ready state if there are not current faulters */
	if (!faulter)
		return Rm_session::State();

	/* return fault information regarding the first faulter of the list */
	return faulter->fault_state();
}


void Rm_session_component::dissolve(Rm_client *cl)
{
	Lock::Guard lock_guard(_lock);
	_pager_ep->dissolve(cl);
	_clients.remove(cl);
	destroy(&_client_slab, cl);
}


static Dataspace_capability _type_deduction_helper(Dataspace_capability cap) {
	return cap; }


Rm_session_component::Rm_session_component(Rpc_entrypoint   *ds_ep,
                                           Rpc_entrypoint   *thread_ep,
                                           Allocator        *md_alloc,
                                           size_t            ram_quota,
                                           Pager_entrypoint *pager_ep,
                                           addr_t            vm_start,
                                           size_t            vm_size)
:
	_ds_ep(ds_ep), _thread_ep(thread_ep),
	_md_alloc(md_alloc, ram_quota),
	_client_slab(&_md_alloc), _ref_slab(&_md_alloc),
	_map(&_md_alloc), _pager_ep(pager_ep),
	_ds(this, vm_size), _ds_cap(_type_deduction_helper(ds_ep->manage(&_ds)))
{
	/* configure managed VM area */
	_map.add_range(vm_start, vm_size);
}


Rm_session_component::~Rm_session_component()
{
	_lock.lock();

	/* revoke dataspace representation */
	_ds_ep->dissolve(&_ds);

	/* remove all faulters with pending page faults at this rm session */
	while (Rm_faulter *faulter = _faulters.first()) {
		_lock.unlock();
		faulter->dissolve_from_faulting_rm_session();
		_lock.lock();
	}

	/* remove all clients */
	while (Rm_client *cl = _client_slab.first_object()) {
		_pager_ep->dissolve(cl);
		_lock.unlock();
		cl->dissolve_from_faulting_rm_session();
		_lock.lock();
		_clients.remove(cl);
		destroy(&_client_slab, cl);
	}

	/* detach all regions */
	while (Rm_region_ref *r = _ref_slab.first_object()) {
		_lock.unlock();
		detach((void *)r->region()->base());
		_lock.lock();
	}

	_lock.unlock();
}
