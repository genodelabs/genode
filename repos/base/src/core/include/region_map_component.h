/*
 * \brief  Region map interface
 * \author Christian Helmuth
 * \author Norman Feske
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__REGION_MAP_COMPONENT_H_
#define _CORE__INCLUDE__REGION_MAP_COMPONENT_H_

/* Genode includes */
#include <base/mutex.h>
#include <base/capability.h>
#include <pager.h>
#include <base/allocator_avl.h>
#include <base/session_label.h>
#include <base/synced_allocator.h>
#include <base/signal.h>
#include <base/rpc_server.h>
#include <base/heap.h>
#include <util/list.h>
#include <util/fifo.h>
#include <pd_session/pd_session.h>

/* core includes */
#include <platform.h>
#include <dataspace_component.h>
#include <util.h>
#include <log2_range.h>
#include <address_space.h>

/* base-internal includes */
#include <base/internal/stack_area.h>

namespace Core {
	class  Region_map_detach;
	class  Rm_region;
	struct Fault;
	class  Cpu_thread_component;
	class  Dataspace_component;
	class  Region_map_component;
	class  Rm_client;
	class  Rm_faulter;
	class  Rm_session_component;
}


class Core::Region_map_detach : Interface
{
	public:

		virtual void detach(Region_map::Local_addr) = 0;
		virtual void unmap_region(addr_t base, size_t size) = 0;
};


/**
 * Representation of a single entry of a region map
 *
 * Each 'Rm_region' is associated with one dataspace and makes a portion
 * of this dataspace visible in a address space of a region map.
 * All 'Rm_regions' to which one and the same dataspace is attached to, are
 * organized in a linked list. The head of the list is a member of the
 * 'Dataspace_component'.
 */
class Core::Rm_region : public List<Rm_region>::Element
{
	public:

		struct Attr
		{
			addr_t base;
			size_t size;
			bool   write;
			bool   exec;
			off_t  off;
			bool   dma;

			void print(Output &out) const
			{
				Genode::print(out, "[", Hex(base), ",", Hex(base + size - 1), " "
				              "(r", write ? "w" : "-", exec ? "x" : "-", ") "
				              "offset: ",    Hex(off), dma ? " DMA" : "");
			}
		};

	private:

		Dataspace_component  &_dsc;
		Region_map_detach    &_rm;

		Attr const _attr;

	public:

		Rm_region(Dataspace_component &dsc, Region_map_detach &rm, Attr attr)
		:
			_dsc(dsc), _rm(rm), _attr(attr)
		{ }

		addr_t                    base() const { return _attr.base;  }
		size_t                    size() const { return _attr.size;  }
		bool                     write() const { return _attr.write; }
		bool                executable() const { return _attr.exec;  }
		off_t                   offset() const { return _attr.off;   }
		bool                       dma() const { return _attr.dma;   }
		Dataspace_component &dataspace() const { return _dsc; }
		Region_map_detach          &rm() const { return _rm;  }

		Addr_range range() const { return { .start = _attr.base,
		                                    .end   = _attr.base + _attr.size - 1 }; }

		void print(Output &out) const { Genode::print(out, _attr); }
};


struct Core::Fault
{
	Addr       hotspot; /* page-fault address */
	Access     access;  /* reason for the fault, used to detect violations */
	Rwx        rwx;     /* mapping rights, downgraded by 'within_' methods */
	Addr_range bounds;  /* limits of the fault's coordinate system */

	bool write_access() const { return access == Access::WRITE; }
	bool exec_access()  const { return access == Access::EXEC;  }

	/**
	 * Translate fault information to region-relative coordinates
	 */
	Fault within_region(Rm_region const &region) const
	{
		return Fault {
			.hotspot = hotspot.reduced_by(region.base()),
			.access  = access,
			.rwx     = { .w = rwx.w && region.write(),
			             .x = rwx.x && region.executable() },
			.bounds  = bounds.intersected(region.range())
			                 .reduced_by(region.base())
		};
	}

	/**
	 * Translate fault information to coordinates within a sub region map
	 */
	Fault within_sub_region_map(addr_t offset, size_t region_map_size) const
	{
		return {
			.hotspot = hotspot.increased_by(offset),
			.access  = access,
			.rwx     = rwx,
			.bounds  = bounds.intersected({ 0, region_map_size })
			                 .increased_by(offset)
		};
	};

	/**
	 * Translate fault information to physical coordinates for memory mapping
	 */
	Fault within_ram(addr_t offset, Dataspace_component::Attr dataspace) const
	{
		return {
			.hotspot = hotspot.increased_by(offset)
			                  .increased_by(dataspace.base),
			.access  = access,
			.rwx     = { .w = rwx.w && dataspace.writeable,
			             .x = rwx.x },
			.bounds  = bounds.increased_by(offset)
			                 .intersected({ 0, dataspace.size })
			                 .increased_by(dataspace.base)
		};
	};

	void print(Output &out) const { Genode::print(out, access, " at address ", hotspot); }
};


/**
 * Member of faulter list
 *
 * Each 'Rm_client' can fault not only at the region map that it is member
 * of but also on any other region map used as a nested dataspace. If a
 * 'Rm_client' faults, it gets enqueued at the leaf region map that
 * detected the fault and waits for this region map to resolve the fault.
 * For example, the dataspace manager that resolves the faults for the
 * nested dataspace exported to its client. Because each region map must
 * be able to handle faults by arbitrary clients (not only its own
 * clients), it maintains the list head of faulters.
 */
class Core::Rm_faulter : Fifo<Rm_faulter>::Element, Interface
{
	private:

		Pager_object                   &_pager_object;
		Mutex                           _mutex { };
		Weak_ptr<Region_map_component>  _faulting_region_map { };
		Region_map::State               _fault_state { };

		friend class Fifo<Rm_faulter>;

	public:

		/**
		 * Constructor
		 *
		 * \param Pager_object  pager object that corresponds to the faulter
		 *
		 * Currently, there is only one pager in core.
		 */
		explicit Rm_faulter(Pager_object &pager_object) : _pager_object(pager_object) { }

		/**
		 * Assign fault state
		 */
		void fault(Region_map_component &faulting_region_map,
		           Region_map::State     fault_state);

		/**
		 * Disassociate faulter from the faulted region map
		 *
		 * This function must be called when destructing region maps
		 * to prevent dangling pointers in '_faulters' lists.
		 */
		void dissolve_from_faulting_region_map(Region_map_component &);

		/**
		 * Return true if page fault occurred in specified address range
		 */
		bool fault_in_addr_range(addr_t addr, size_t size) {
			return (_fault_state.addr >= addr) && (_fault_state.addr <= addr + size - 1); }

		/**
		 * Return fault state as exported via the region-map interface
		 */
		Region_map::State fault_state() { return _fault_state; }

		/**
		 * Wake up faulter by answering the pending page fault
		 */
		void continue_after_resolved_fault();
};


/**
 * Member role of region map
 *
 * A region map can be used as address space for any number of threads. This
 * class represents the thread's role as member of this address space.
 */
class Core::Rm_client : public Pager_object, public Rm_faulter,
                        private List<Rm_client>::Element
{
	private:

		friend class List<Rm_client>;

		Region_map_component &_region_map;

	public:

		/**
		 * Constructor
		 *
		 * \param rm        address-space region map of the client
		 * \param badge     pager-object badge used of identifying the client
		 *                  when a page fault occurs
		 * \param location  affinity to physical CPU
		 */
		Rm_client(Cpu_session_capability cpu_session,
		          Thread_capability thread,
		          Region_map_component &rm, unsigned long badge,
		          Affinity::Location location,
		          Session_label     const &pd_label,
		          Cpu_session::Name const &name)
		:
			Pager_object(cpu_session, thread, badge, location, pd_label, name),
			Rm_faulter(static_cast<Pager_object &>(*this)), _region_map(rm)
		{ }

		Pager_result pager(Ipc_pager &pager) override;

		/**
		 * Return region map that the RM client is member of
		 */
		Region_map_component &member_rm() { return _region_map; }
};


class Core::Region_map_component : private Weak_object<Region_map_component>,
                                   public  Rpc_object<Region_map>,
                                   private List<Region_map_component>::Element,
                                   public  Region_map_detach
{
	public:

		enum class With_mapping_result { RESOLVED, RECURSION_LIMIT, NO_REGION,
		                                 REFLECTED, WRITE_VIOLATION, EXEC_VIOLATION };

	private:

		friend class List<Region_map_component>;

		Session::Diag const _diag;

		Rpc_entrypoint &_ds_ep;
		Rpc_entrypoint &_thread_ep;
		Rpc_entrypoint &_session_ep;

		Allocator &_md_alloc;

		Signal_context_capability _fault_sigh { };

		Address_space  *_address_space { nullptr };

		/*
		 * Noncopyable
		 */
		Region_map_component(Region_map_component const &);
		Region_map_component &operator = (Region_map_component const &);


		/*********************
		 ** Paging facility **
		 *********************/

		class Rm_region_ref : public List<Rm_region_ref>::Element
		{
			private:

				Rm_region *_region;

			public:

				Rm_region_ref(Rm_region *region) : _region(region) { }

				Rm_region* region() const { return _region; }
		};


		class Rm_dataspace_component : public Dataspace_component
		{
			private:

				Native_capability _rm_cap { };

			public:

				/**
				 * Constructor
				 */
				Rm_dataspace_component(size_t size)
				:
					Dataspace_component(size, 0, CACHED, false, 0)
				{
					_managed = true;
				}


				/***********************************
				 ** Dataspace component interface **
				 ***********************************/

				Native_capability sub_rm() override { return _rm_cap; }
				void sub_rm(Native_capability cap) { _rm_cap = cap; }
		};

		/*
		 * Dimension slab allocator for regions such that backing store is
		 * allocated at the granularity of pages.
		 */
		typedef Tslab<Rm_region_ref, get_page_size() - Sliced_heap::meta_data_size()>
		        Ref_slab;

		Allocator_avl_tpl<Rm_region>  _map;          /* region map for attach,
		                                                detach, pagefaults */
		Fifo<Rm_faulter>              _faulters { }; /* list of threads that faulted at
		                                                the region map and wait
		                                                for fault resolution */
		List<Rm_client>               _clients  { }; /* list of RM clients using this region map */
		Mutex mutable                 _mutex    { }; /* mutex for map and list */
		Pager_entrypoint             &_pager_ep;
		Rm_dataspace_component        _ds;           /* dataspace representation of region map */
		Dataspace_capability          _ds_cap;

		struct Recursion_limit { unsigned value; };

		/**
		 * Resolve region at a given fault address
		 *
		 * /param resolved_fn   functor called with the resolved region and the
		 *                      region-relative fault information
		 *
		 * Called recursively when resolving a page fault in nested region maps.
		 */
		With_mapping_result _with_region_at_fault(Recursion_limit const  recursion_limit,
		                                          Fault           const &fault,
		                                          auto            const &resolved_fn,
		                                          auto            const &reflect_fn)
		{
			using Result = With_mapping_result;

			if (recursion_limit.value == 0)
				return Result::RECURSION_LIMIT;

			Mutex::Guard lock_guard(_mutex);

			/* lookup region and dataspace */
			Rm_region const * const region_ptr = _map.metadata((void*)fault.hotspot.value);

			auto reflect_fault = [&] (Result result)
			{
				if (!_fault_sigh.valid())
					return result;   /* not reflected to user land */

				reflect_fn(*this, fault);
				return Result::REFLECTED;  /* omit diagnostics */
			};

			if (!region_ptr)
				return reflect_fault(Result::NO_REGION);

			Rm_region const &region = *region_ptr;

			/* fault information relative to 'region' */
			Fault const relative_fault = fault.within_region(region);

			Dataspace_component &dataspace = region.dataspace();

			Native_capability managed_ds_cap = dataspace.sub_rm();

			/* region refers to a regular dataspace */
			if (!managed_ds_cap.valid()) {

				bool const writeable = relative_fault.rwx.w
				                    && dataspace.writeable();

				bool const write_violation = relative_fault.write_access()
				                         && !writeable;

				bool const exec_violation  = relative_fault.exec_access()
				                         && !relative_fault.rwx.x;

				if (write_violation) return reflect_fault(Result::WRITE_VIOLATION);
				if (exec_violation)  return reflect_fault(Result::EXEC_VIOLATION);

				return resolved_fn(region, relative_fault);
			}

			/* traverse into managed dataspace */
			Fault const sub_region_map_relative_fault =
				relative_fault.within_sub_region_map(region.offset(),
				                                     dataspace.size());

			Result result = Result::NO_REGION;
			_session_ep.apply(managed_ds_cap, [&] (Region_map_component *rmc_ptr) {
				if (rmc_ptr)
					result = rmc_ptr->_with_region_at_fault({ recursion_limit.value - 1 },
					                                        sub_region_map_relative_fault,
					                                        resolved_fn, reflect_fn); });
			return result;
		}

		/*
		 * Returns the core-local address behind region 'r'
		 */
		addr_t _core_local_addr(Rm_region & r);

		struct Attach_attr
		{
			size_t size;
			off_t  offset;
			bool   use_local_addr;
			addr_t local_addr;
			bool   executable;
			bool   writeable;
			bool   dma;
		};

		Local_addr _attach(Dataspace_capability, Attach_attr);

	public:

		/*
		 * Unmaps a memory area from all address spaces referencing it.
		 *
		 * \param base base address of region to unmap
		 * \param size size of region to unmap
		 */
		void unmap_region(addr_t base, size_t size) override;

		/**
		 * Constructor
		 *
		 * The object calls 'ep.manage' for itself on construction.
		 */
		Region_map_component(Rpc_entrypoint   &ep,
		                     Allocator        &md_alloc,
		                     Pager_entrypoint &pager_ep,
		                     addr_t            vm_start,
		                     size_t            vm_size,
		                     Session::Diag     diag);

		~Region_map_component();

		using Weak_object<Region_map_component>::weak_ptr;
		friend class Locked_ptr<Region_map_component>;

		bool equals(Weak_ptr<Region_map_component> const &other)
		{
			return (this == static_cast<Region_map_component *>(other.obj()));
		}

		void address_space(Address_space *space) { _address_space = space; }
		Address_space *address_space() { return _address_space; }

		/**
		 * Register fault
		 *
		 * This function is called by the pager to schedule a page fault
		 * for resolution.
		 *
		 * \param  faulter  faulting region-manager client
		 * \param  pf_addr  page-fault address
		 * \param  pf_type  type of page fault (read/write/execute)
		 */
		void fault(Rm_faulter &faulter, addr_t pf_addr,
		           Region_map::State::Fault_type pf_type);

		/**
		 * Dissolve faulter from region map
		 */
		void discard_faulter(Rm_faulter &faulter, bool do_lock);

		/**
		 * Return the dataspace representation of this region map
		 */
		Rm_dataspace_component &dataspace_component() { return _ds; }

		/**
		 * Call 'apply_fn' with resolved mapping information for given fault
		 *
		 * /param apply_fn    functor called with a 'Mapping' that is suitable
		 *                    for resolving given the 'fault'
		 * /param reflect_fn  functor called to reflect a missing mapping
		 *                    to user space if a fault handler is registered
		 */
		With_mapping_result with_mapping_for_fault(Fault const &fault,
		                                           auto  const &apply_fn,
		                                           auto  const &reflect_fn)
		{
			return _with_region_at_fault(Recursion_limit { 5 }, fault,
				[&] (Rm_region const &region, Fault const &region_relative_fault)
				{
					Dataspace_component &dataspace = region.dataspace();

					Fault const ram_relative_fault =
						region_relative_fault.within_ram(region.offset(), dataspace.attr());

					Log2_range src_range { ram_relative_fault.hotspot };
					Log2_range dst_range { fault.hotspot };

					src_range = src_range.constrained(ram_relative_fault.bounds);

					Log2 const common_size = Log2_range::common_log2(dst_range,
					                                                 src_range);
					Log2 const map_size = kernel_constrained_map_size(common_size);

					src_range = src_range.constrained(map_size);
					dst_range = dst_range.constrained(map_size);

					if (!src_range.valid() || !dst_range.valid()) {
						error("invalid mapping");
						return With_mapping_result::NO_REGION;
					}

					Mapping const mapping {
						.dst_addr       = dst_range.base.value,
						.src_addr       = src_range.base.value,
						.size_log2      = map_size.log2,
						.cached         = dataspace.cacheability() == CACHED,
						.io_mem         = dataspace.io_mem(),
						.dma_buffer     = region.dma(),
						.write_combined = dataspace.cacheability() == WRITE_COMBINED,
						.writeable      = ram_relative_fault.rwx.w,
						.executable     = ram_relative_fault.rwx.x
					};

					apply_fn(mapping);

					return With_mapping_result::RESOLVED;
				},
				reflect_fn
			);
		}

		/**
		 * Register thread as user of the region map as its address space
		 *
		 * Called at thread-construction time only.
		 */
		void add_client(Rm_client &);
		void remove_client(Rm_client &);

		using Attach_dma_result = Pd_session::Attach_dma_result;

		Attach_dma_result attach_dma(Dataspace_capability, addr_t);


		/**************************
		 ** Region map interface **
		 **************************/

		Local_addr       attach        (Dataspace_capability, size_t, off_t,
		                                bool, Local_addr, bool, bool) override;
		void             detach        (Local_addr) override;
		void             fault_handler (Signal_context_capability handler) override;
		State            state         () override;

		Dataspace_capability dataspace () override { return _ds_cap; }
};

#endif /* _CORE__INCLUDE__REGION_MAP_COMPONENT_H_ */
