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
#include <address_space.h>

/* base-internal includes */
#include <base/internal/stack_area.h>

namespace Core {
	class Cpu_thread_component;
	class Dataspace_component;
	class Region_map_component;
	class Region_map_detach;
	class Rm_client;
	class Rm_region;
	class Rm_faulter;
	class Rm_session_component;
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

		int pager(Ipc_pager &pager) override;

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
	private:

		friend class List<Region_map_component>;

		Session::Diag const _diag;

		Rpc_entrypoint &_ds_ep;
		Rpc_entrypoint &_thread_ep;
		Rpc_entrypoint &_session_ep;

		Allocator &_md_alloc;

		Signal_transmitter _fault_notifier { };  /* notification mechanism for
		                                            region-manager faults */

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
		Mutex                         _mutex    { }; /* mutex for map and list */
		Pager_entrypoint             &_pager_ep;
		Rm_dataspace_component        _ds;           /* dataspace representation of region map */
		Dataspace_capability          _ds_cap;

		template <typename F>
		auto _apply_to_dataspace(addr_t addr, F const &f, addr_t offset,
		                         unsigned level, addr_t dst_region_size)
		-> typename Trait::Functor<decltype(&F::operator())>::Return_type
		{
			using Functor = Trait::Functor<decltype(&F::operator())>;
			using Return_type = typename Functor::Return_type;

			Mutex::Guard lock_guard(_mutex);

			/* skip further lookup when reaching the recursion limit */
			if (!level)
				return f(this, nullptr, 0, 0, dst_region_size);

			/* lookup region and dataspace */
			Rm_region *region        = _map.metadata((void*)addr);
			Dataspace_component *dsc = region ? &region->dataspace()
			                                  : nullptr;

			if (region && dst_region_size > region->size())
				dst_region_size = region->size();

			/* calculate offset in dataspace */
			addr_t ds_offset = region ? (addr - region->base()
			                             + region->offset()) : 0;

			/* check for nested dataspace */
			Native_capability cap = dsc ? dsc->sub_rm()
			                            : Native_capability();

			if (!cap.valid())
				return f(this, region, ds_offset, offset, dst_region_size);

			/* in case of a nested dataspace perform a recursive lookup */
			auto lambda = [&] (Region_map_component *rmc) -> Return_type
			{
				if (rmc)
					return rmc->_apply_to_dataspace(ds_offset, f,
					                                offset + region->base() - region->offset(),
					                                --level,
					                                dst_region_size);

				return f(nullptr, nullptr, ds_offset, offset, dst_region_size);
			};
			return _session_ep.apply(cap, lambda);
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

		class Fault_area;

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
		 * Apply a function to dataspace attached at a given address
		 *
		 * /param addr   address where the dataspace is attached
		 * /param f      functor or lambda to apply
		 */
		template <typename F>
		auto apply_to_dataspace(addr_t addr, F f)
		-> typename Trait::Functor<decltype(&F::operator())>::Return_type
		{
			enum { RECURSION_LIMIT = 5 };

			return _apply_to_dataspace(addr, f, 0, RECURSION_LIMIT, ~0UL);
		}

		/**
		 * Register thread as user of the region map as its address space
		 *
		 * Called at thread-construction time only.
		 */
		void add_client(Rm_client &);
		void remove_client(Rm_client &);

		/**
		 * Create mapping item to be placed into the page table
		 */
		static Mapping create_map_item(Region_map_component *region_map,
		                               Rm_region            &region,
		                               addr_t                ds_offset,
		                               addr_t                region_offset,
		                               Dataspace_component  &dsc,
		                               addr_t, addr_t);

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
