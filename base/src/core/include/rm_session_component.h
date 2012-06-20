/*
 * \brief  RM session interface
 * \author Christian Helmuth
 * \author Norman Feske
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__RM_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__RM_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/stdint.h>
#include <base/lock.h>
#include <base/capability.h>
#include <base/pager.h>
#include <base/allocator_avl.h>
#include <base/allocator_guard.h>
#include <base/signal.h>
#include <base/rpc_server.h>
#include <util/list.h>

/* core includes */
#include <platform.h>
#include <dataspace_component.h>
#include <util.h>

namespace Genode {


	class Dataspace_component;
	class Rm_session_component;
	class Rm_client;

	/**
	 * Representation of a single entry of a region-manager session
	 *
	 * Each 'Rm_region' is associated with one dataspace and makes a portion
	 * of this dataspace visible in a address space of a region-manager session.
	 * All 'Rm_regions' to which one and the same dataspace is attached to, are
	 * organized in a linked list. The head of the list is a member of the
	 * 'Dataspace_component'.
	 */
	class Rm_region : public List<Rm_region>::Element
	{
		private:

			addr_t                _base;
			size_t                _size;
			bool                  _write;

			Dataspace_component  *_dsc;
			off_t                 _off;

			Rm_session_component *_session; /* corresponding region manager
			                                   session */

		public:

			/**
			 * Default constructor - invalid region
			 */
			Rm_region() { }

			Rm_region(addr_t base, size_t size, bool write,
			          Dataspace_component *dsc, off_t offset,
			          Rm_session_component *session)
			: _base(base), _size(size), _write(write),
			  _dsc(dsc), _off(offset), _session(session) { }


			/***************
			 ** Accessors **
			 ***************/

			addr_t                    base() const { return _base;    }
			size_t                    size() const { return _size;    }
			bool                     write() const { return _write;   }
			Dataspace_component* dataspace() const { return _dsc;     }
			off_t                   offset() const { return _off;     }
			Rm_session_component*  session() const { return _session; }
	};


	/**
	 * Member of faulter list
	 *
	 * Each 'Rm_client' can fault not only at the RM session that it is member
	 * of but also on any other RM session used as a nested dataspace. If a
	 * 'Rm_client' faults, it gets enqueued at the leaf RM session that
	 * detected the fault and waits for this RM session to resolve the fault.
	 * For example, the dataspace manager that resolves the faults for the
	 * nested dataspace exported to its client. Because each RM session must
	 * be able to handle faults by arbitrary clients (not only its own
	 * clients), it maintains the list head of faulters.
	 */
	class Rm_faulter : public List<Rm_faulter>::Element
	{
		private:

			Pager_object         *_pager_object;
			Lock                  _lock;
			Rm_session_component *_faulting_rm_session;
			Rm_session::State     _fault_state;

		public:

			/**
			 * Constructor
			 *
			 * \param Pager_object  pager object that corresponds to the faulter
			 *
			 * Currently, there is only one pager in core.
			 */
			Rm_faulter(Pager_object *pager_object) :
				_pager_object(pager_object), _faulting_rm_session(0) { }

			/**
			 * Assign fault state
			 */
			void fault(Rm_session_component *faulting_rm_session,
			           Rm_session::State     fault_state);

			/**
			 * Disassociate faulter from the faulted region-manager session
			 *
			 * This function must be called when destructing region-manager
			 * sessions to prevent dangling pointers in '_faulters' lists.
			 */
			void dissolve_from_faulting_rm_session();

			/**
			 * Return true if page fault occurred in specified address range
			 */
			bool fault_in_addr_range(addr_t addr, size_t size) {
				return (_fault_state.addr >= addr) && (_fault_state.addr <= addr + size - 1); }

			/**
			 * Return fault state as exported via the rm-session interface
			 */
			Rm_session::State fault_state() { return _fault_state; }

			/**
			 * Wake up faulter by answering the pending page fault
			 */
			void continue_after_resolved_fault();
	};


	/**
	 * Member role of region manager session
	 *
	 * A region-manager session can be used as address space for any number
	 * of threads (region-manager clients). This class represents the client's
	 * role as member of this address space.
	 */
	class Rm_member : public List<Rm_client>
	{
		private:

			Rm_session_component *_rm_session;

		public:

			/**
			 * Constructor
			 */
			Rm_member(Rm_session_component *rm_session): _rm_session(rm_session) { }

			/**
			 * Return region-manager session that the RM client is member of
			 */
			Rm_session_component *member_rm_session() { return _rm_session; }
	};


	class Rm_client : public Pager_object, public Rm_member, public Rm_faulter,
	                  public List<Rm_client>::Element
	{
		public:

			/**
			 * Constructor
			 *
			 * \param session  RM session to which the client belongs
			 * \param badge    pager-object badge used of identifying the client
			 *                 when a page-fault occurs
			 */
			Rm_client(Rm_session_component *session, unsigned long badge) :
				Pager_object(badge), Rm_member(session), Rm_faulter(this) { }

			int pager(Ipc_pager &pager);

			/**
			 * Flush memory mappings for the specified virtual address range
			 */
			void unmap(addr_t core_local_base, addr_t virt_base, size_t size);
	};


	class Rm_session_component : public Rpc_object<Rm_session>
	{
		private:

			Rpc_entrypoint *_ds_ep;
			Rpc_entrypoint *_thread_ep;

			Allocator_guard    _md_alloc;
			Signal_transmitter _fault_notifier;  /* notification mechanism for
			                                        region-manager faults */

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

					Rm_session_component *_rm_session_component;

				public:

					/**
					 * Constructor
					 */
					Rm_dataspace_component(Rm_session_component *rsc, size_t size)
					:
						Dataspace_component(size, 0, false, false, 0),
						_rm_session_component(rsc) { _managed = true; }


					/***********************************
					 ** Dataspace component interface **
					 ***********************************/

					Rm_session_component *sub_rm_session() { return _rm_session_component; }
			};


			Tslab<Rm_client, 1024>        _client_slab;  /* backing store for
			                                                client structures */
			Tslab<Rm_region_ref, 1024>    _ref_slab;     /* backing store for
			                                                region list */
			Allocator_avl_tpl<Rm_region>  _map;          /* region map for attach,
			                                                detach, pagefaults */
			List<Rm_region_ref>           _regions;      /* region list for destruction */

			List<Rm_faulter>              _faulters;     /* list of threads that faulted at
			                                                the region-manager session and wait
			                                                for fault resolution */
			List<Rm_client>               _clients;      /* list of RM clients using this RM
			                                                session */
			Lock                          _lock;         /* lock for map and list */
			Pager_entrypoint             *_pager_ep;
			Rm_dataspace_component        _ds;           /* dataspace representation of region map */
			Dataspace_capability          _ds_cap;

		public:

			/**
			 * Constructor
			 */
			Rm_session_component(Rpc_entrypoint   *ds_ep,
			                     Rpc_entrypoint   *thread_ep,
			                     Allocator        *md_alloc,
			                     size_t            ram_quota,
			                     Pager_entrypoint *pager_ep,
			                     addr_t            vm_start,
			                     size_t            vm_size);

			~Rm_session_component();

			class Fault_area;

			/**
			 * Reversely lookup dataspace and offset matching the specified address
			 *
			 * \return true  lookup succeeded
			 */
			bool reverse_lookup(addr_t                dst_base,
			                    Fault_area           *dst_fault_region,
			                    Dataspace_component **src_dataspace,
			                    Fault_area           *src_fault_region);

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
			void fault(Rm_faulter *faulter, addr_t pf_addr,
			           Rm_session::Fault_type pf_type);

			/**
			 * Dissolve faulter from region-manager session
			 */
			void discard_faulter(Rm_faulter *faulter);

			List<Rm_client> *clients() { return &_clients; }
			
			/**
			 * Return the dataspace representation of this session
			 */
			Rm_dataspace_component *dataspace_component() { return &_ds; }

			/**
			 * Register quota donation at allocator guard
			 */
			void upgrade_ram_quota(size_t ram_quota) { _md_alloc.upgrade(ram_quota); }

			/**
			 * Dissolves client from region-manager session
			 */
			void dissolve(Rm_client *cl);


			/**************************************
			 ** Region manager session interface **
			 **************************************/

			Local_addr       attach        (Dataspace_capability, size_t, off_t, bool, Local_addr, bool);
			void             detach        (Local_addr);
			Pager_capability add_client    (Thread_capability);
			void             fault_handler (Signal_context_capability handler);
			State            state         ();
			Dataspace_capability dataspace () { return _ds_cap; }
	};
}

#endif /* _CORE__INCLUDE__RM_SESSION_COMPONENT_H_ */
