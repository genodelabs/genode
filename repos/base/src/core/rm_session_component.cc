/*
 * \brief  Implementation of the RM session interface
 * \author Christian Helmuth
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2006-07-17
 *
 * FIXME arg_string and quota missing
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
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
				addr_t fpage_mask = ~((1UL << try_size_log2) - 1);
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
	using Fault_area = Rm_session_component::Fault_area;

	Rm_session::Fault_type pf_type = pager.is_write_fault() ? Rm_session::WRITE_FAULT
	                                                        : Rm_session::READ_FAULT;
	addr_t pf_addr = pager.fault_addr();
	addr_t pf_ip   = pager.fault_ip();

	if (verbose_page_faults)
		print_page_fault("page fault", pf_addr, pf_ip, pf_type, badge());

	auto lambda = [&] (Rm_session_component *rm_session,
	                   Rm_region            *region,
	                   addr_t                ds_offset,
	                   addr_t                region_offset) -> int
	{
		Dataspace_component * dsc = region ? region->dataspace() : nullptr;
		if (!dsc) {

			/*
			 * We found no attachment at the page-fault address and therefore have
			 * to reflect the page fault as region-manager fault. The signal
			 * handler is then expected to request the state of the region-manager
			 * session.
			 */

			/* print a warning if it's no managed-dataspace */
			if (rm_session == member_rm_session())
				print_page_fault("no RM attachment", pf_addr, pf_ip,
				                 pf_type, badge());

			/* register fault at responsible region-manager session */
			if (rm_session)
				rm_session->fault(this, pf_addr - region_offset, pf_type);

			/* there is no attachment return an error condition */
			return 1;
		}

		addr_t ds_base = dsc->map_src_addr();
		Fault_area src_fault_area(ds_base + ds_offset);
		Fault_area dst_fault_area(pf_addr);
		src_fault_area.constrain(ds_base, dsc->size());
		dst_fault_area.constrain(region_offset + region->base(), region->size());

		/*
		 * Determine mapping size compatible with source and destination,
		 * and apply platform-specific constraint of mapping sizes.
		 */
		size_t map_size_log2 = dst_fault_area.common_size_log2(dst_fault_area,
		                                                       src_fault_area);
		map_size_log2 = constrain_map_size_log2(map_size_log2);

		src_fault_area.constrain(map_size_log2);
		dst_fault_area.constrain(map_size_log2);
		if (!src_fault_area.valid() || !dst_fault_area.valid())
			PERR("Invalid mapping");

		/*
		 * Check if dataspace is compatible with page-fault type
		 */
		if (pf_type == Rm_session::WRITE_FAULT && !dsc->writable()) {

			/* attempted there is no attachment return an error condition */
			print_page_fault("attempted write at read-only memory",
			                 pf_addr, pf_ip, pf_type, badge());

			/* register fault at responsible region-manager session */
			rm_session->fault(this, src_fault_area.fault_addr(), pf_type);
			return 2;
		}

		Mapping mapping(dst_fault_area.base(), src_fault_area.base(),
		                dsc->cacheability(), dsc->is_io_mem(),
		                map_size_log2, dsc->writable());

		/*
		 * On kernels with a mapping database, the 'dsc' dataspace is a leaf
		 * dataspace that corresponds to a virtual address range within core. To
		 * prepare the answer for the page fault, we make sure that this range is
		 * locally mapped in core. On platforms that support map operations of
		 * pages that are not locally mapped, the 'map_core_local' function may be
		 * empty.
		 */
		if (!dsc->is_io_mem())
			mapping.prepare_map_operation();

		/* answer page fault with a flex-page mapping */
		pager.set_reply_mapping(mapping);
		return 0;
	};
	return member_rm_session()->apply_to_dataspace(pf_addr, lambda);
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

	_pager_object->unresolved_page_fault_occurred();
}


void Rm_faulter::dissolve_from_faulting_rm_session(Rm_session_component * caller)
{
	/* serialize access */
	Lock::Guard lock_guard(_lock);

	if (_faulting_rm_session)
		_faulting_rm_session->discard_faulter(this, _faulting_rm_session != caller);

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
	if (offset < 0 || align_addr(offset, get_page_size_log2()) != offset)
		throw Invalid_args();

	auto lambda = [&] (Dataspace_component *dsc) {
		/* check dataspace validity */
		if (!dsc) throw Invalid_dataspace();

		if (!size)
			size = dsc->size() - offset;

		/* work with page granularity */
		size = align_addr(size, get_page_size_log2());

		/* deny creation of regions larger then the actual dataspace */
		if (dsc->size() < size + offset)
			throw Invalid_args();

		/* allocate region for attachment */
		void *r = 0;
		if (use_local_addr) {
			switch (_map.alloc_addr(size, local_addr).value) {

			case Range_allocator::Alloc_return::OUT_OF_METADATA:
				throw Out_of_metadata();

			case Range_allocator::Alloc_return::RANGE_CONFLICT:
				throw Region_conflict();

			case Range_allocator::Alloc_return::OK:
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
				if (((dsc->map_src_addr() + offset) & ((1UL << align_log2) - 1)) != 0)
					continue;

				/* try allocating the align region */
				Range_allocator::Alloc_return alloc_return =
					_map.alloc_aligned(size, &r, align_log2);

				if (alloc_return.is_ok())
					break;
				else if (alloc_return.value == Range_allocator::Alloc_return::OUT_OF_METADATA) {
					_map.free(r);
					throw Out_of_metadata();
				}
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
			     (Dataspace_component *)dsc, dsc->phys_addr(), dsc->size(),
			     offset, (addr_t)r, (addr_t)r + size);

		/* check if attach operation resolves any faulting region-manager clients */
		for (Rm_faulter *faulter = _faulters.head(); faulter; ) {

			/* remember next pointer before possibly removing current list element */
			Rm_faulter *next = faulter->next();

			if (faulter->fault_in_addr_range((addr_t)r, size)) {
				_faulters.remove(faulter);
				faulter->continue_after_resolved_fault();
			}

			faulter = next;
		}

		return r;
	};

	return _ds_ep->apply(ds_cap, lambda);
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
	Rm_client *prev_rc = 0;
	Rm_client *rc = _clients.first();
	for (; rc; rc = rc->List<Rm_client>::Element::next(), prev_rc = rc) {

		/*
		 * XXX Unmapping managed dataspaces on kernels, which take a core-
		 *     local virtual address as unmap argument is not supported yet.
		 *     This is the case for Fiasco, Pistachio, and NOVA. On those
		 *     kernels, the unmap operation must be issued for each leaf
		 *     dataspace the managed dataspace is composed of. For kernels with
		 *     support for directed unmap (OKL4), unmap can be
		 *     simply applied for the contiguous virtual address region in the
		 *     client.
		 */
		if (!platform()->supports_direct_unmap()
		 && dsc->is_managed() && dsc->core_local_addr() == 0) {
			PWRN("unmapping of managed dataspaces not yet supported");
			break;
		}

		/*
		 * Don't unmap from the same address space twice. If multiple threads
		 * reside in one PD, each thread will have a corresponding 'Rm_client'
		 * object. Consequenlty, an unmap operation referring to the PD is
		 * issued multiple times, one time for each thread. By comparing the
		 * membership to the thread's respective address spaces, we reduce
		 * superfluous unmap operations.
		 *
		 * Note that the list of 'Rm_client' object may contain threads of
		 * different address spaces in any order. So superfluous unmap
		 * operations can still happen if 'Rm_client' objects of one PD are
		 * interleaved with 'Rm_client' objects of another PD. In practice,
		 * however, this corner case is rare.
		 */
		if (prev_rc && prev_rc->has_same_address_space(*rc))
			continue;

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
	unsigned long badge;
	Affinity::Location location;
	Weak_ptr<Address_space> address_space;

	{
		/* lookup thread and setup correct parameters */
		auto lambda = [&] (Cpu_thread_component *cpu_thread) {
			if (!cpu_thread) throw Invalid_thread();

			/* determine identification of client when faulting */
			badge = cpu_thread->platform_thread()->pager_object_badge();

			/* determine cpu affinity of client thread */
			location = cpu_thread->platform_thread()->affinity();

			address_space = cpu_thread->platform_thread()->address_space();
			if (!Locked_ptr<Address_space>(address_space).is_valid())
				throw Unbound_thread();
		};
		_thread_ep->apply(thread, lambda);
	}

	/* serialize access */
	Lock::Guard lock_guard(_lock);

	Rm_client *cl;
	try { cl = new(&_client_slab) Rm_client(this, badge, address_space, location); }
	catch (Allocator::Out_of_memory) { throw Out_of_metadata(); }
	catch (Cpu_session::Thread_creation_failed) { throw Out_of_metadata(); }
	catch (Thread_base::Stack_alloc_failed) { throw Out_of_metadata(); }

	_clients.insert(cl);

	return Pager_capability(_pager_ep->manage(cl));
}


void Rm_session_component::remove_client(Pager_capability pager_cap)
{
	Rm_client *client;

	auto lambda = [&] (Rm_client *cl) {
		client = cl;

		if (!client) return;

		/*
		 * Rm_client is derived from Pager_object. If the Pager_object is also
		 * derived from Thread_base then the Rm_client object must be
		 * destructed without holding the rm_session_object lock. The native
		 * platform specific Thread_base implementation has to take care that
		 * all in-flight page handling requests are finished before
		 * destruction. (Either by waiting until the end of or by
		 * <deadlock free> cancellation of the last in-flight request.
		 * This operation can also require taking the rm_session_object lock.
		 */
		{
			Lock::Guard lock_guard(_lock);
			_clients.remove(client);
		}

		/* call platform specific dissolve routines */
		_pager_ep->dissolve(client);

		{
			Lock::Guard lock_guard(_lock);
			client->dissolve_from_faulting_rm_session(this);
		}
	};
	_pager_ep->apply(pager_cap, lambda);

	destroy(&_client_slab, client);
}


void Rm_session_component::fault(Rm_faulter *faulter, addr_t pf_addr,
                                 Rm_session::Fault_type pf_type)
{
	/* remember fault state in faulting thread */
	faulter->fault(this, Rm_session::State(pf_type, pf_addr));

	/* enqueue faulter */
	_faulters.enqueue(faulter);

	/* issue fault signal */
	_fault_notifier.submit();
}


void Rm_session_component::discard_faulter(Rm_faulter *faulter, bool do_lock)
{
	if (do_lock) {
		Lock::Guard lock_guard(_lock);
		_faulters.remove(faulter);
	} else
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
	Rm_faulter *faulter = _faulters.head();

	/* return ready state if there are not current faulters */
	if (!faulter)
		return Rm_session::State();

	/* return fault information regarding the first faulter of the list */
	return faulter->fault_state();
}

static Dataspace_capability _type_deduction_helper(Dataspace_capability cap) {
	return cap; }


Rm_session_component::Rm_session_component(Rpc_entrypoint   *ds_ep,
                                           Rpc_entrypoint   *thread_ep,
                                           Rpc_entrypoint   *session_ep,
                                           Allocator        *md_alloc,
                                           size_t            ram_quota,
                                           Pager_entrypoint *pager_ep,
                                           addr_t            vm_start,
                                           size_t            vm_size)
:
	_ds_ep(ds_ep), _thread_ep(thread_ep), _session_ep(session_ep),
	_md_alloc(md_alloc, ram_quota),
	_client_slab(&_md_alloc), _ref_slab(&_md_alloc),
	_map(&_md_alloc), _pager_ep(pager_ep),
	_ds(align_addr(vm_size, get_page_size_log2())),
	_ds_cap(_type_deduction_helper(ds_ep->manage(&_ds)))
{
	/* configure managed VM area */
	_map.add_range(vm_start, align_addr(vm_size, get_page_size_log2()));
}


Rm_session_component::~Rm_session_component()
{
	/* dissolve all clients from pager entrypoint */
	Rm_client *cl;
	do {
		{
			Lock::Guard lock_guard(_lock);
			cl = _clients.first();
			if (!cl) break;

			_clients.remove(cl);
		}

		/* call platform specific dissolve routines */
		_pager_ep->dissolve(cl);
	} while (cl);

	/* detach all regions */
	Rm_region_ref *ref;
	do {
		void * local_addr;
		{
			Lock::Guard lock_guard(_lock);
			ref = _ref_slab.first_object();
			if (!ref) break;
			local_addr = reinterpret_cast<void *>(ref->region()->base());
		}
		detach(local_addr);
	} while (ref);

	/* revoke dataspace representation */
	_ds_ep->dissolve(&_ds);

	/* serialize access */
	_lock.lock();

	/* remove all faulters with pending page faults at this rm session */
	while (Rm_faulter *faulter = _faulters.head())
		faulter->dissolve_from_faulting_rm_session(this);

	/* remove all clients, invalidate rm_client pointers in cpu_thread objects */
	while (Rm_client *cl = _client_slab.raw()->first_object()) {
		cl->dissolve_from_faulting_rm_session(this);

		Thread_capability thread_cap = cl->thread_cap();
		if (thread_cap.valid())
			/* invalidate thread cap in rm_client object */
			cl->thread_cap(Thread_capability());

		_lock.unlock();

		/* lookup thread and reset pager pointer */
		auto lambda = [&] (Cpu_thread_component *cpu_thread) {
			if (cpu_thread && (cpu_thread->platform_thread()->pager() == cl))
				cpu_thread->platform_thread()->pager(0);
		};
		_thread_ep->apply(thread_cap, lambda);

		destroy(&_client_slab, cl);

		_lock.lock();
	}

	_lock.unlock();
}
