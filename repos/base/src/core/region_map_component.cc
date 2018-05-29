/*
 * \brief  Implementation of the region map
 * \author Christian Helmuth
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/lock.h>
#include <util/arg_string.h>
#include <util/misc_math.h>

/* core includes */
#include <util.h>
#include <cpu_session_component.h>
#include <region_map_component.h>
#include <dataspace_component.h>

static const bool verbose             = false;
static const bool verbose_page_faults = false;


struct Genode::Region_map_component::Fault_area
{
	addr_t _fault_addr = 0;
	addr_t _base       = 0;
	size_t _size_log2  = 0;

	addr_t _upper_bound() const {
		return (_size_log2 == ~0UL) ? ~0UL : (_base + (1UL << _size_log2) - 1); }

	/**
	 * Default constructor, constructs invalid fault area
	 */
	Fault_area() { }

	/**
	 * Constructor, fault area spans the maximum address-space size
	 */
	Fault_area(addr_t fault_addr) :
		_fault_addr(fault_addr), _size_log2(~0UL) { }

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
			if (try_base + (1UL << try_size_log2) - 1 > region_base + region_size - 1)
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

		_base = _fault_addr & ~((1UL << size_log2) - 1);
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
		for (; n < min_size_log2 && !(diff & (1UL << n)); n++);

		return n;
	}

	addr_t fault_addr() const { return _fault_addr; }
	addr_t base()       const { return _base; }
	bool   valid()      const { return _size_log2 > 0; }
};


using namespace Genode;

static void print_page_fault(char const *msg,
                             addr_t pf_addr,
                             addr_t pf_ip,
                             Region_map::State::Fault_type pf_type,
                             Pager_object const &obj)
{
	log(msg, " (",
	    pf_type == Region_map::State::WRITE_FAULT ? "WRITE" :
	    pf_type == Region_map::State::READ_FAULT ? "READ" : "EXEC",
	    " pf_addr=", Hex(pf_addr), " pf_ip=", Hex(pf_ip), " from ", obj, ") ");
}


/***********************
 ** Region-map client **
 ***********************/

/*
 * This code is executed by the page-fault handler thread.
 */

int Rm_client::pager(Ipc_pager &pager)
{
	Region_map::State::Fault_type pf_type = pager.write_fault() ? Region_map::State::WRITE_FAULT
	                                                            : Region_map::State::READ_FAULT;
	if (pager.exec_fault())
		pf_type = Region_map::State::EXEC_FAULT;

	addr_t pf_addr = pager.fault_addr();
	addr_t pf_ip   = pager.fault_ip();

	if (verbose_page_faults)
		print_page_fault("page fault", pf_addr, pf_ip, pf_type, *this);

	auto lambda = [&] (Region_map_component *region_map,
	                   Rm_region            *region,
	                   addr_t const          ds_offset,
	                   addr_t const          region_offset,
	                   addr_t const          dst_region_size) -> int
	{
		Dataspace_component * dsc = region ? region->dataspace() : nullptr;
		if (!dsc) {

			/*
			 * We found no attachment at the page-fault address and therefore have
			 * to reflect the page fault as region-manager fault. The signal
			 * handler is then expected to request the state of the region map.
			 */

			/* print a warning if it's no managed-dataspace */
			if (region_map == member_rm())
				print_page_fault("no RM attachment", pf_addr, pf_ip,
				                 pf_type, *this);

			/* register fault at responsible region map */
			if (region_map)
				region_map->fault(this, pf_addr - region_offset, pf_type);

			/* there is no attachment return an error condition */
			return 1;
		}

		/*
		 * Check if dataspace is compatible with page-fault type
		 */
		if (pf_type == Region_map::State::WRITE_FAULT &&
		    (!region->write() || !dsc->writable())) {

			print_page_fault("attempted write at read-only memory",
			                 pf_addr, pf_ip, pf_type, *this);

			/* register fault at responsible region map */
			if (region_map)
				region_map->fault(this, pf_addr - region_offset, pf_type);
			return 2;
		}

		if (pf_type == Region_map::State::EXEC_FAULT) {

			print_page_fault("attempted exec at non-executable memory",
			                 pf_addr, pf_ip, pf_type, *this);

			/* register fault at responsible region map */
			if (region_map)
				region_map->fault(this, pf_addr - region_offset, pf_type);
			return 3;
		}

		Mapping mapping = Region_map_component::create_map_item(region_map,
		                                                        region,
		                                                        ds_offset,
		                                                        region_offset,
		                                                        dsc, pf_addr,
		                                                        dst_region_size);

		/*
		 * On kernels with a mapping database, the 'dsc' dataspace is a leaf
		 * dataspace that corresponds to a virtual address range within core. To
		 * prepare the answer for the page fault, we make sure that this range is
		 * locally mapped in core. On platforms that support map operations of
		 * pages that are not locally mapped, the 'map_core_local' function may be
		 * empty.
		 */
		if (!dsc->io_mem())
			mapping.prepare_map_operation();

		/* answer page fault with a flex-page mapping */
		pager.set_reply_mapping(mapping);
		return 0;
	};
	return member_rm()->apply_to_dataspace(pf_addr, lambda);
}


/*************
 ** Faulter **
 *************/

void Rm_faulter::fault(Region_map_component *faulting_region_map,
                       Region_map::State     fault_state)
{
	Lock::Guard lock_guard(_lock);

	_faulting_region_map = faulting_region_map->weak_ptr();
	_fault_state         = fault_state;

	_pager_object->unresolved_page_fault_occurred();
}


void Rm_faulter::dissolve_from_faulting_region_map(Region_map_component * caller)
{
	/* serialize access */
	Lock::Guard lock_guard(_lock);

	enum { DO_LOCK = true };
	if (caller->equals(_faulting_region_map)) {
		caller->discard_faulter(this, !DO_LOCK);
	} else {
		Locked_ptr<Region_map_component> locked_ptr(_faulting_region_map);

		if (locked_ptr.valid())
			locked_ptr->discard_faulter(this, DO_LOCK);
	}

	_faulting_region_map = Genode::Weak_ptr<Genode::Region_map_component>();
}


void Rm_faulter::continue_after_resolved_fault()
{
	Lock::Guard lock_guard(_lock);

	_pager_object->wake_up();
	_faulting_region_map = Genode::Weak_ptr<Genode::Region_map_component>();
	_fault_state = Region_map::State();
}


/**************************
 ** Region-map component **
 **************************/

Mapping Region_map_component::create_map_item(Region_map_component *,
                                              Rm_region            *region,
                                              addr_t const          ds_offset,
                                              addr_t const          region_offset,
                                              Dataspace_component  *dsc,
                                              addr_t const          page_addr,
                                              addr_t const          dst_region_size)
{
	addr_t ds_base = dsc->map_src_addr();
	Fault_area src_fault_area(ds_base + ds_offset);
	Fault_area dst_fault_area(page_addr);
	src_fault_area.constrain(ds_base, dsc->size());
	dst_fault_area.constrain(region_offset + region->base(), dst_region_size);

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
		error("invalid mapping");

	return Mapping(dst_fault_area.base(), src_fault_area.base(),
	               dsc->cacheability(), dsc->io_mem(),
	               map_size_log2, region->write() && dsc->writable(),
	               region->executable());
};


Region_map::Local_addr
Region_map_component::attach(Dataspace_capability ds_cap, size_t size,
                             off_t offset, bool use_local_addr,
                             Region_map::Local_addr local_addr,
                             bool executable, bool writeable)
{
	/* serialize access */
	Lock::Guard lock_guard(_lock);

	/* offset must be positive and page-aligned */
	if (offset < 0 || align_addr(offset, get_page_size_log2()) != offset)
		throw Region_conflict();

	auto lambda = [&] (Dataspace_component *dsc) {
		/* check dataspace validity */
		if (!dsc) throw Invalid_dataspace();

		size_t const off = offset;
		if (off >= dsc->size())
			throw Region_conflict();

		if (!size)
			size = dsc->size() - offset;

		/* work with page granularity */
		size = align_addr(size, get_page_size_log2());

		/* deny creation of regions larger then the actual dataspace */
		if (dsc->size() < size + offset)
			throw Region_conflict();

		/* allocate region for attachment */
		void *attach_at = 0;
		if (use_local_addr) {
			switch (_map.alloc_addr(size, local_addr).value) {

			case Range_allocator::Alloc_return::OUT_OF_METADATA:
				throw Out_of_ram();

			case Range_allocator::Alloc_return::RANGE_CONFLICT:
				throw Region_conflict();

			case Range_allocator::Alloc_return::OK:
				attach_at = local_addr;
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
					_map.alloc_aligned(size, &attach_at, align_log2);

				typedef Range_allocator::Alloc_return Alloc_return;

				switch (alloc_return.value) {
				case Alloc_return::OK:              break; /* switch */
				case Alloc_return::OUT_OF_METADATA: throw Out_of_ram();
				case Alloc_return::RANGE_CONFLICT:  continue; /* for loop */
				}
				break; /* for loop */

			}

			if (align_log2 < get_page_size_log2())
				throw Region_conflict();
		}

		/* store attachment info in meta data */
		try {
			_map.metadata(attach_at, Rm_region((addr_t)attach_at, size,
			                                   dsc->writable() && writeable,
			                                   dsc, offset, this, executable));
		}
		catch (Allocator_avl_tpl<Rm_region>::Assign_metadata_failed) {
			error("failed to store attachment info");
			throw Invalid_dataspace();
		}
		Rm_region *region = _map.metadata(attach_at);

		/* inform dataspace about attachment */
		dsc->attached_to(region);

		/* check if attach operation resolves any faulting region-manager clients */
		for (Rm_faulter *faulter = _faulters.head(); faulter; ) {

			/* remember next pointer before possibly removing current list element */
			Rm_faulter *next = faulter->next();

			if (faulter->fault_in_addr_range((addr_t)attach_at, size)) {
				_faulters.remove(faulter);
				faulter->continue_after_resolved_fault();
			}

			faulter = next;
		}

		return attach_at;
	};

	return _ds_ep->apply(ds_cap, lambda);
}


static void unmap_managed(Region_map_component *rm, Rm_region *region, int level)
{
	for (Rm_region *managed = rm->dataspace_component()->regions()->first();
	     managed;
	     managed = managed->List<Rm_region>::Element::next()) {

		if (managed->base() - managed->offset() >= region->base() - region->offset()
		    && managed->base() - managed->offset() + managed->size()
		       <= region->base() - region->offset() + region->size())
			unmap_managed(managed->rm(), managed, level + 1);

		if (!managed->rm()->address_space())
			continue;

		/* found a leaf node (here a leaf is an Region_map whose dataspace has no regions) */

		Address_space::Core_local_addr core_local
			= { region->dataspace()->core_local_addr() + region->offset() };
		managed->rm()->address_space()->flush(managed->base() + region->base() -
		                                      managed->offset(),
		                                      region->size(), core_local);
	}
}


void Region_map_component::detach(Local_addr local_addr)
{
	/* serialize access */
	Lock::Guard lock_guard(_lock);

	/* read meta data for address */
	Rm_region *region_ptr = _map.metadata(local_addr);

	if (!region_ptr) {
		if (_diag.enabled)
			warning("detach: no attachment at ", (void *)local_addr);
		return;
	}

	if ((region_ptr->base() != static_cast<addr_t>(local_addr)) && _diag.enabled)
		warning("detach: ", static_cast<void *>(local_addr), " is not "
		        "the beginning of the region ", Hex(region_ptr->base()));

	Dataspace_component *dsc = region_ptr->dataspace();
	if (!dsc && _diag.enabled)
		warning("detach: region of ", this, " may be inconsistent!");

	/* inform dataspace about detachment */
	dsc->detached_from(region_ptr);

	/*
	 * Create local copy of region data because the '_map.metadata' of the
	 * region will become unavailable as soon as we call '_map.free' below.
	 */
	Rm_region region = *region_ptr;

	/*
	 * Deallocate region on platforms that support unmap
	 *
	 * On platforms without support for unmap, the
	 * same virtual address must not be reused. Hence, we never mark used
	 * regions as free.
	 *
	 * We unregister the region from region map prior unmapping the pages to
	 * make sure that page faults occurring immediately after the unmap
	 * refer to an empty region not to the dataspace, which we just removed.
	 */
	if (platform()->supports_unmap())
		_map.free(reinterpret_cast<void *>(region.base()));

	/*
	 * This function gets called from the destructor of 'Dataspace_component',
	 * which iterates through all regions the dataspace is attached to. One
	 * particular case is the destruction of an 'Region_map_component' and its
	 * contained managed dataspace ('_ds') member. The type of this member is
	 * derived from 'Dataspace_component' and provides the 'sub_region_map'
	 * function, which can normally be used to distinguish managed dataspaces
	 * from leaf dataspaces. However, at destruction time of the '_dsc' base
	 * class, the vtable entry of 'sub_region_map' already points to the
	 * base-class's function. Hence, we cannot check the return value of this
	 * function to determine if the dataspace is a managed dataspace. Instead,
	 * we introduced a dataspace member '_managed' with the non-virtual accessor
	 * function 'managed'.
	 */

	if (_address_space) {

		if (!platform()->supports_direct_unmap() && dsc->managed() &&
		    dsc->core_local_addr() == 0) {

			if (_diag.enabled)
				warning("unmapping of managed dataspaces not yet supported");

		} else {
			Address_space::Core_local_addr core_local
				= { dsc->core_local_addr() + region.offset() };
			_address_space->flush(region.base(), region.size(), core_local);
		}
	}

	/*
	 * If region map is used as nested dataspace, unmap this dataspace from all
	 * region maps.
	 */
	unmap_managed(this, &region, 1);
}


void Region_map_component::add_client(Rm_client &rm_client)
{
	Lock::Guard lock_guard(_lock);

	_clients.insert(&rm_client);
}


void Region_map_component::remove_client(Rm_client &rm_client)
{
	Lock::Guard lock_guard(_lock);

	_clients.remove(&rm_client);
	rm_client.dissolve_from_faulting_region_map(this);
}


void Region_map_component::fault(Rm_faulter *faulter, addr_t pf_addr,
                                 Region_map::State::Fault_type pf_type)
{
	/* remember fault state in faulting thread */
	faulter->fault(this, Region_map::State(pf_type, pf_addr));

	/* enqueue faulter */
	_faulters.enqueue(faulter);

	/* issue fault signal */
	_fault_notifier.submit();
}


void Region_map_component::discard_faulter(Rm_faulter *faulter, bool do_lock)
{
	if (do_lock) {
		Lock::Guard lock_guard(_lock);
		_faulters.remove(faulter);
	} else
		_faulters.remove(faulter);
}


void Region_map_component::fault_handler(Signal_context_capability handler)
{
	_fault_notifier.context(handler);
}


Region_map::State Region_map_component::state()
{
	/* serialize access */
	Lock::Guard lock_guard(_lock);

	/* pick one of the currently faulted threads */
	Rm_faulter *faulter = _faulters.head();

	/* return ready state if there are not current faulters */
	if (!faulter)
		return Region_map::State();

	/* return fault information regarding the first faulter of the list */
	return faulter->fault_state();
}


static Dataspace_capability
_type_deduction_helper(Dataspace_capability cap) { return cap; }


Region_map_component::Region_map_component(Rpc_entrypoint   &ep,
                                           Allocator        &md_alloc,
                                           Pager_entrypoint &pager_ep,
                                           addr_t            vm_start,
                                           size_t            vm_size,
                                           Session::Diag     diag)
:
	_diag(diag), _ds_ep(&ep), _thread_ep(&ep), _session_ep(&ep),
	_md_alloc(md_alloc),
	_map(&_md_alloc), _pager_ep(&pager_ep),
	_ds(align_addr(vm_size, get_page_size_log2())),
	_ds_cap(_type_deduction_helper(_ds_ep->manage(&_ds)))
{
	/* configure managed VM area */
	_map.add_range(vm_start, align_addr(vm_size, get_page_size_log2()));

	Capability<Region_map> cap = ep.manage(this);
	_ds.sub_rm(cap);
}


Region_map_component::~Region_map_component()
{
	_ds_ep->dissolve(this);

	lock_for_destruction();

	/* dissolve all clients from pager entrypoint */
	Rm_client *cl;
	do {
		Cpu_session_capability cpu_session_cap;
		Thread_capability      thread_cap;
		{
			Lock::Guard lock_guard(_lock);
			cl = _clients.first();
			if (!cl) break;

			cl->dissolve_from_faulting_region_map(this);

			cpu_session_cap = cl->cpu_session_cap();
			thread_cap      = cl->thread_cap();

			_clients.remove(cl);
		}

		/* destroy thread */
		auto lambda = [&] (Cpu_session_component *cpu_session) {
			if (cpu_session)
				cpu_session->kill_thread(thread_cap);
		};
		_thread_ep->apply(cpu_session_cap, lambda);
	} while (cl);

	/* detach all regions */
	while (true) {
		addr_t out_addr = 0;

		{
			Lock::Guard lock_guard(_lock);
			if (!_map.any_block_addr(&out_addr))
				break;
		}

		detach(out_addr);
	}

	/* revoke dataspace representation */
	_ds_ep->dissolve(&_ds);
}
