/*
 * \brief  Implementation of the region map
 * \author Christian Helmuth
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/arg_string.h>
#include <util/misc_math.h>

/* core includes */
#include <util.h>
#include <cpu_session_component.h>
#include <region_map_component.h>
#include <dataspace_component.h>

using namespace Core;


/*
 * This code is executed by the page-fault handler thread.
 */

Pager_object::Pager_result Rm_client::pager(Ipc_pager &pager)
{
	Fault const fault {
		.hotspot = { pager.fault_addr() },
		.access  = pager.write_fault() ? Access::WRITE
		         : pager.exec_fault()  ? Access::EXEC
		         :                       Access::READ,
		.rwx     = Rwx::rwx(),
		.bounds  = { .start = 0, .end  = ~0UL },
	};

	using Result = Region_map_component::With_mapping_result;

	Result const result = member_rm().with_mapping_for_fault(fault,

		[&] (Mapping const &mapping)
		{
			/*
			 * On kernels with a mapping database, the leaf dataspace
			 * corresponds to a virtual address range within core. To prepare
			 * the answer for the page fault, we make sure that this range is
			 * locally mapped in core.
			 */
			if (!mapping.io_mem)
				mapping.prepare_map_operation();

			/* answer page fault with a flex-page mapping */
			pager.set_reply_mapping(mapping);
		},

		[&] (Region_map_component &rm, Fault const &fault) /* reflect to user space */
		{
			using Type = Region_map::Fault::Type;
			Type const type = (fault.access == Access::READ)  ? Type::READ
			                : (fault.access == Access::WRITE) ? Type::WRITE
			                :                                   Type::EXEC;
			/* deliver fault info to responsible region map */
			rm.fault(*this, { .type = type, .addr = fault.hotspot.value });
		}
	);

	if (result == Result::RESOLVED)
		return Pager_result::CONTINUE;

	/*
	 * Error diagnostics
	 */

	struct Origin
	{
		addr_t ip;
		Pager_object &obj;

		void print(Output &out) const
		{
			Genode::print(out, "by ", obj, " ip=", Hex(ip));
		}
	} origin { pager.fault_ip(), *this };

	switch (result) {
	case Result::RESOLVED:
	case Result::REFLECTED:
		break;

	case Result::RECURSION_LIMIT:
		error("giving up on unexpectedly deep memory-mapping structure");
		error(fault, " ", origin);
		break;

	case Result::NO_REGION:
		error("illegal ", fault, " ", origin);
		break;

	case Result::WRITE_VIOLATION:
	case Result::EXEC_VIOLATION:
		error(fault.access, " violation at address ",
		      fault.hotspot, " ", origin);
		break;
	}
	return Pager_result::STOP;
}


/*************
 ** Faulter **
 *************/

void Rm_faulter::fault(Region_map_component &faulting_region_map,
                       Region_map::Fault     fault)
{
	Mutex::Guard lock_guard(_mutex);

	_faulting_region_map = faulting_region_map.weak_ptr();
	_fault               = fault;

	_pager_object.unresolved_page_fault_occurred();
}


void Rm_faulter::dissolve_from_faulting_region_map(Region_map_component &caller)
{
	/* serialize access */
	Mutex::Guard lock_guard(_mutex);

	enum { DO_LOCK = true };
	if (caller.equals(_faulting_region_map)) {
		caller.discard_faulter(*this, !DO_LOCK);
	} else {
		Locked_ptr<Region_map_component> locked_ptr(_faulting_region_map);

		if (locked_ptr.valid())
			locked_ptr->discard_faulter(*this, DO_LOCK);
	}

	_faulting_region_map = Weak_ptr<Core::Region_map_component>();
}


void Rm_faulter::continue_after_resolved_fault()
{
	Mutex::Guard lock_guard(_mutex);

	_pager_object.wake_up();
	_faulting_region_map = Weak_ptr<Core::Region_map_component>();
	_fault = { };
}


/**************************
 ** Region-map component **
 **************************/

Region_map::Attach_result
Region_map_component::_attach(Dataspace_capability ds_cap, Attach_attr const core_attr)
{
	Attr const attr = core_attr.attr;

	/* serialize access */
	Mutex::Guard lock_guard(_mutex);

	/* offset must be page-aligned */
	if (align_addr(attr.offset, get_page_size_log2()) != attr.offset)
		return Attach_error::REGION_CONFLICT;

	auto lambda = [&] (Dataspace_component *dsc) -> Attach_result {

		using Alloc_error = Range_allocator::Alloc_error;

		/* check dataspace validity */
		if (!dsc)
			return Attach_error::INVALID_DATASPACE;

		unsigned const min_align_log2 = get_page_size_log2();

		size_t const ds_size = dsc->size();

		if (attr.offset >= ds_size)
			return Attach_error::REGION_CONFLICT;

		size_t size = attr.size ? attr.size : ds_size - attr.offset;

		/* work with page granularity */
		size = align_addr(size, min_align_log2);

		/* deny creation of regions larger then the actual dataspace */
		if (ds_size < size + attr.offset)
			return Attach_error::REGION_CONFLICT;

		/* allocate region for attachment */
		bool at_defined = false;
		addr_t at { };
		if (attr.use_at) {
			Alloc_error error = Alloc_error::DENIED;
			_map.alloc_addr(size, attr.at).with_result(
				[&] (void *ptr)     { at = addr_t(ptr); at_defined = true; },
				[&] (Alloc_error e) { error = e; });

			if (error == Alloc_error::OUT_OF_RAM)  return Attach_error::OUT_OF_RAM;
			if (error == Alloc_error::OUT_OF_CAPS) return Attach_error::OUT_OF_CAPS;

		} else {

			/*
			 * Find optimal alignment for new region. First try natural alignment.
			 * If that is not possible, try again with successively less alignment
			 * constraints.
			 */
			size_t align_log2 = log2(size);
			if (align_log2 >= sizeof(void *)*8)
				align_log2 = min_align_log2;

			for (; !at_defined && (align_log2 >= min_align_log2); align_log2--) {

				/*
				 * Don't use an alignment higher than the alignment of the backing
				 * store. The backing store would constrain the mapping size
				 * anyway such that a higher alignment of the region is of no use.
				 */
				if (((dsc->map_src_addr() + attr.offset) & ((1UL << align_log2) - 1)) != 0)
					continue;

				/* try allocating the aligned region */
				Alloc_error error = Alloc_error::DENIED;
				_map.alloc_aligned(size, unsigned(align_log2)).with_result(
					[&] (void *ptr) { at = addr_t(ptr); at_defined = true; },
					[&] (Alloc_error e) { error = e; });

				if (error == Alloc_error::OUT_OF_RAM)  return Attach_error::OUT_OF_RAM;
				if (error == Alloc_error::OUT_OF_CAPS) return Attach_error::OUT_OF_CAPS;
			}
		}
		if (!at_defined)
			return Attach_error::REGION_CONFLICT;

		Rm_region::Attr const region_attr
		{
			.base  = at,
			.size  = size,
			.write = attr.writeable,
			.exec  = attr.executable,
			.off   = attr.offset,
			.dma   = core_attr.dma,
		};

		/* store attachment info in meta data */
		try {
			_map.construct_metadata((void *)at, *dsc, *this, region_attr);
		}
		catch (Allocator_avl_tpl<Rm_region>::Assign_metadata_failed) {
			error("failed to store attachment info");
			return Attach_error::INVALID_DATASPACE;
		}

		/* inform dataspace about attachment */
		Rm_region * const region_ptr = _map.metadata((void *)at);
		if (region_ptr)
			dsc->attached_to(*region_ptr);

		/* check if attach operation resolves any faulting region-manager clients */
		_faulters.for_each([&] (Rm_faulter &faulter) {
			if (faulter.fault_in_addr_range(at, size)) {
				_faulters.remove(faulter);
				faulter.continue_after_resolved_fault();
			}
		});

		return Range { .start = at, .num_bytes = size };
	};

	return _ds_ep.apply(ds_cap, lambda);
}


addr_t Region_map_component::_core_local_addr(Rm_region & region)
{
	addr_t result = 0;

	region.with_dataspace([&] (Dataspace_component &dataspace) {
		/**
		 * If this region references a managed dataspace,
		 * we have to recursively request the core-local address
		 */
		if (dataspace.sub_rm().valid()) {
			auto lambda = [&] (Region_map_component * rmc) -> addr_t
			{
				/**
				 * It is possible that there is no dataspace attached
				 * inside the managed dataspace, in that case return zero.
				 */
				Rm_region * r = rmc ? rmc->_map.metadata((void*)region.offset())
				                    : nullptr;;
				return (r && !r->reserved()) ? rmc->_core_local_addr(*r) : 0;
			};
			result = _session_ep.apply(dataspace.sub_rm(), lambda);
			return;
		}

		/* return core-local address of dataspace + region offset */
		result = dataspace.core_local_addr() + region.offset();
	});

	return result;
}


void Region_map_component::unmap_region(addr_t base, size_t size)
{
	if (address_space()) address_space()->flush(base, size, { 0 });

	/**
	 * Iterate over all regions that reference this region map
	 * as managed dataspace
	 */
	for (Rm_region * r = dataspace_component().regions().first();
	     r; r = r->List<Rm_region>::Element::next()) {

		/**
		 * Check whether the region referencing the managed dataspace
		 * and the region to unmap overlap
		 */
		addr_t ds_base = max((addr_t)r->offset(), base);
		addr_t ds_end  = min((addr_t)r->offset()+r->size(), base+size);
		size_t ds_size = ds_base < ds_end ? ds_end - ds_base : 0;

		/* if size is not zero, there is an overlap */
		if (ds_size)
			r->rm().unmap_region(r->base() + ds_base - r->offset(), ds_size);
	}
}


Region_map::Attach_result
Region_map_component::attach(Dataspace_capability ds_cap, Attr const &attr)
{
	return _attach(ds_cap, { .attr = attr, .dma = false });
}


Region_map_component::Attach_dma_result
Region_map_component::attach_dma(Dataspace_capability ds_cap, addr_t at)
{
	Attach_attr const attr {
		.attr = {
			.size       = { },
			.offset     = { },
			.use_at     = true,
			.at         = at,
			.executable = false,
			.writeable  = true,
		},
		.dma            = true,
	};

	using Attach_dma_error = Pd_session::Attach_dma_error;

	return _attach(ds_cap, attr).convert<Attach_dma_result>(
		[&] (Range) { return Pd_session::Attach_dma_ok(); },
		[&] (Attach_error e) {
			switch (e) {
			case Attach_error::OUT_OF_RAM:  return Attach_dma_error::OUT_OF_RAM;
			case Attach_error::OUT_OF_CAPS: return Attach_dma_error::OUT_OF_CAPS;
			case Attach_error::REGION_CONFLICT:   break;
			case Attach_error::INVALID_DATASPACE: break;
			}
			return Attach_dma_error::DENIED;
		});
}


void Region_map_component::_reserve_and_flush_unsynchronized(Rm_region &region)
{
	/* inform dataspace about detachment */
	region.with_dataspace([&] (Dataspace_component &dsc) {
		dsc.detached_from(region);
	});

	if (!platform().supports_direct_unmap()) {

		/*
		 * Determine core local address of the region, where necessary.
		 * If we can't retrieve it, it is not possible to unmap on kernels
		 * that do not support direct unmap functionality, therefore return in that
		 * case.
		 * Otherwise calling flush with core_local address zero on kernels that
		 * unmap indirectly via core's address space can lead to illegitime unmaps
		 * of core memory (reference issue #3082)
		 */
		Address_space::Core_local_addr core_local { _core_local_addr(region) };

		/*
		 * We mark the region as reserved prior unmapping the pages to
		 * make sure that page faults occurring immediately after the unmap
		 * do not refer to the dataspace, which we just removed. Since
		 * 'mark_as_reserved()' invalidates the dataspace pointer, it
		 * must be called after '_core_local_addr()'.
		 */
		region.mark_as_reserved();

		if (core_local.value)
			platform_specific().core_pd().flush(0, region.size(), core_local);
	} else {

		/*
		 * We mark the region as reserved prior unmapping the pages to
		 * make sure that page faults occurring immediately after the unmap
		 * do not refer to the dataspace, which we just removed.
		 */
		region.mark_as_reserved();

		/*
		 * Unmap this memory region from all region maps referencing it.
		 */
		unmap_region(region.base(), region.size());
	}
}


/*
 * Flush the region, but keep it reserved until 'detach()' is called.
 */
void Region_map_component::reserve_and_flush(addr_t const at)
{
	/* serialize access */
	Mutex::Guard lock_guard(_mutex);

	_with_region(at, [&] (Rm_region &region) {
		_reserve_and_flush_unsynchronized(region);
	});
}


void Region_map_component::detach_at(addr_t const at)
{
	/* serialize access */
	Mutex::Guard lock_guard(_mutex);

	_with_region(at, [&] (Rm_region &region) {
		if (!region.reserved())
			_reserve_and_flush_unsynchronized(region);
		/* free the reserved region */
		_map.free(reinterpret_cast<void *>(region.base()));
	});
}


void Region_map_component::add_client(Rm_client &rm_client)
{
	Mutex::Guard lock_guard(_mutex);

	_clients.insert(&rm_client);
}


void Region_map_component::remove_client(Rm_client &rm_client)
{
	Mutex::Guard lock_guard(_mutex);

	_clients.remove(&rm_client);
	rm_client.dissolve_from_faulting_region_map(*this);
}


void Region_map_component::fault(Rm_faulter &faulter, Region_map::Fault fault)
{
	/* remember fault state in faulting thread */
	faulter.fault(*this, fault);

	/* enqueue faulter */
	_faulters.enqueue(faulter);

	/* issue fault signal */
	Signal_transmitter(_fault_sigh).submit();
}


void Region_map_component::discard_faulter(Rm_faulter &faulter, bool do_lock)
{
	if (do_lock) {
		Mutex::Guard lock_guard(_mutex);
		_faulters.remove(faulter);
	} else
		_faulters.remove(faulter);
}


void Region_map_component::fault_handler(Signal_context_capability sigh)
{
	_fault_sigh = sigh;
}


Region_map::Fault Region_map_component::fault()
{
	/* serialize access */
	Mutex::Guard lock_guard(_mutex);

	/* return fault information regarding the first faulter */
	Region_map::Fault result { };
	_faulters.head([&] (Rm_faulter &faulter) {
		result = faulter.fault(); });
	return result;
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
	_diag(diag), _ds_ep(ep), _thread_ep(ep), _session_ep(ep),
	_md_alloc(md_alloc),
	_map(&_md_alloc), _pager_ep(pager_ep),
	_ds(align_addr(vm_size, get_page_size_log2())),
	_ds_cap(_type_deduction_helper(_ds_ep.manage(&_ds)))
{
	/* configure managed VM area */
	_map.add_range(vm_start, align_addr(vm_size, get_page_size_log2()));

	Capability<Region_map> cap = ep.manage(this);
	_ds.sub_rm(cap);
}


Region_map_component::~Region_map_component()
{
	lock_for_destruction();

	/*
	 * Normally, detaching ds from all regions maps is done in the
	 * destructor of the dataspace. But we do it here explicitely
	 * so that the regions refering this ds can retrieve it via
	 * there capabilities before it gets dissolved in the next step.
	 */
	_ds.detach_from_rm_sessions();
	_ds_ep.dissolve(this);

	/* dissolve all clients from pager entrypoint */
	Rm_client *cl = nullptr;
	do {
		Cpu_session_capability cpu_session_cap;
		Thread_capability      thread_cap;
		{
			Mutex::Guard lock_guard(_mutex);
			cl = _clients.first();
			if (!cl) break;

			cl->dissolve_from_faulting_region_map(*this);

			cpu_session_cap = cl->cpu_session_cap();
			thread_cap      = cl->thread_cap();

			_clients.remove(cl);
		}

		/* destroy thread */
		auto lambda = [&] (Cpu_session_component *cpu_session) {
			if (cpu_session)
				cpu_session->kill_thread(thread_cap);
		};
		_thread_ep.apply(cpu_session_cap, lambda);
	} while (cl);

	/* detach all regions */
	while (true) {
		addr_t out_addr = 0;

		{
			Mutex::Guard lock_guard(_mutex);
			if (!_map.any_block_addr(&out_addr))
				break;
		}

		detach_at(out_addr);
	}

	/* revoke dataspace representation */
	_ds_ep.dissolve(&_ds);
}
