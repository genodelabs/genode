/*
 * \brief  RM session implementation used by Noux processes
 * \author Norman Feske
 * \author Martin Stein
 * \date   2012-02-22
 *
 * The custom RM implementation is used for recording all RM regions attached
 * to the region-manager session. Using the recorded information, the address-
 * space layout can then be replayed onto a new process created via fork.
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__RM_SESSION_COMPONENT_H_
#define _NOUX__RM_SESSION_COMPONENT_H_

/* Genode includes */
#include <rm_session/connection.h>
#include <base/rpc_server.h>

namespace Noux
{
	static bool verbose_attach = false;

	/**
	 * Server sided back-end of an RM session of a Noux process
	 */
	class Rm_session_component;
}

class Noux::Rm_session_component : public Rpc_object<Rm_session>
{
	private:

		/**
		 * Record of an attached dataspace
		 */
		struct Region : List<Region>::Element, Dataspace_user
		{
			Rm_session_component &rm;
			Dataspace_capability  ds;
			size_t                size;
			off_t                 offset;
			addr_t                local_addr;

			Region(Rm_session_component &rm,
			       Dataspace_capability ds, size_t size,
			       off_t offset, addr_t local_addr)
			:
				rm(rm), ds(ds), size(size), offset(offset),
				local_addr(local_addr)
			{ }

			/**
			 * Return true if region contains specified address
			 */
			bool contains(addr_t addr) const
			{
				return (addr >= local_addr) && (addr <  local_addr + size);
			}

			Region *next_region()
			{
				return List<Region>::Element::next();
			}

			inline void dissolve(Dataspace_info &ds);
		};

		Lock         _region_lock;
		List<Region> _regions;

		Region *_lookup_region_by_addr(addr_t local_addr)
		{
			Region *curr = _regions.first();
			for (; curr; curr = curr->next_region()) {
				if (curr->contains(local_addr))
					return curr;
			}
			return 0;
		}

		/**
		 * Wrapped RM session at core
		 */
		Rm_connection _rm;

		Dataspace_registry &_ds_registry;

	public:

		/**
		 * Constructor
		 */
		Rm_session_component(Dataspace_registry & ds_registry,
		                     addr_t start = ~0UL, size_t size = 0)
		:
			_rm(start, size), _ds_registry(ds_registry)
		{ }

		/**
		 * Destructor
		 */
		~Rm_session_component()
		{
			Region *curr;
			while ((curr = _regions.first()))
				detach(curr->local_addr);
		}

		/**
		 * Return leaf RM session that covers a given address
		 *
		 * \param addr  address that is covered by the requested RM session
		 */
		Rm_session_capability lookup_rm_session(addr_t const addr)
		{
			/* if there's no region that could be a sub RM then we're a leaf */
			Region * const region = _lookup_region_by_addr(addr);
			if (!region) { return cap(); }

			/* if there is no info for the region it can't be a sub RM */
			Dataspace_capability ds_cap = region->ds;
			typedef Object_pool<Dataspace_info>::Guard Info_guard;
			Info_guard info(_ds_registry.lookup_info(ds_cap));
			if (!info) { return cap(); }

			/* ask the dataspace info for an appropriate sub RM */
			addr_t const region_base = region->local_addr;
			addr_t const region_off = region->offset;
			addr_t const sub_addr = addr - region_base + region_off;
			Rm_session_capability sub_rm = info->lookup_rm_session(sub_addr);

			/* if the result is invalid the dataspace is no sub RM */
			if (!sub_rm.valid()) { return cap(); }
			return sub_rm;
		}

		/**
		 * Replay attachments onto specified RM session
		 *
		 * \param dst_ram      backing store used for allocating the
		 *                     the copies of RAM dataspaces
		 * \param ds_registry  dataspace registry used for keeping track
		 *                     of newly created dataspaces
		 * \param ep           entrypoint used to serve the RPC interface
		 *                     of forked managed dataspaces
		 */
		void replay(Ram_session_capability dst_ram,
		            Rm_session_capability  dst_rm,
		            Dataspace_registry    &ds_registry,
		            Rpc_entrypoint        &ep)
		{
			Lock::Guard guard(_region_lock);
			for (Region *curr = _regions.first(); curr; curr = curr->next_region()) {

				Dataspace_capability ds;

				Object_pool<Dataspace_info>::Guard info(_ds_registry.lookup_info(curr->ds));

				if (info) {

					ds = info->fork(dst_ram, ds_registry, ep);

					/*
					 * XXX We could detect dataspaces that are attached
					 *     more than once. For now, we create a new fork
					 *     for each attachment.
					 */

				} else {

					PWRN("replay: missing ds_info for dataspace at addr 0x%lx",
					     curr->local_addr);

					/*
					 * If the dataspace is not a RAM dataspace, assume that
					 * it's a ROM dataspace.
					 *
					 * XXX Handle ROM dataspaces explicitly. For once, we
					 *     need to make sure that they remain available
					 *     until the child process exits even if the parent
					 *     process exits earlier. Furthermore, we would
					 *     like to detect unexpected dataspaces.
					 */
					ds = curr->ds;
				}

				if (!ds.valid()) {
					PERR("replay: Error while forking dataspace");
					continue;
				}

				Rm_session_client(dst_rm).attach(ds, curr->size,
				                                 curr->offset,
				                                 true,
				                                 curr->local_addr);
			}
		}

		void poke(addr_t dst_addr, void const *src, size_t len)
		{
			Dataspace_capability ds_cap;
			addr_t               local_addr;

			{
				Lock::Guard guard(_region_lock);

				Region *region = _lookup_region_by_addr(dst_addr);
				if (!region) {
					PERR("poke: no region at 0x%lx", dst_addr);
					return;
				}

				/*
				 * Test if start and end address occupied by the object
				 * type refers to the same region.
				 */
				if (region != _lookup_region_by_addr(dst_addr + len - 1)) {
					PERR("attempt to write beyond region boundary");
					return;
				}

				if (region->offset) {
					PERR("poke: writing to region with offset is not supported");
					return;
				}

				ds_cap = region->ds;
				local_addr = region->local_addr;
			}

			Object_pool<Dataspace_info>::Guard info(_ds_registry.lookup_info(ds_cap));
			if (!info) {
				PERR("attempt to write to unknown dataspace type");
				for (;;);
				return;
			}

			info->poke(dst_addr - local_addr, src, len);
		}


		/**************************
		 ** RM session interface **
		 **************************/

		Local_addr attach(Dataspace_capability ds,
		                  size_t size = 0, off_t offset = 0,
		                  bool use_local_addr = false,
		                  Local_addr local_addr = (addr_t)0,
		                  bool executable = false)
		{
			/*
			 * Rm_session subtracts offset from size if size is 0
			 */
			if (size == 0) size = Dataspace_client(ds).size() - offset;

			for (;;) {
				try {
					local_addr = _rm.attach(ds, size, offset, use_local_addr,
					                        local_addr, executable);
					break;
				} catch (Rm_session::Out_of_metadata) {
					Genode::env()->parent()->upgrade(_rm, "ram_quota=8096");
				}
			}

			Region * region = new (env()->heap())
			                  Region(*this, ds, size, offset, local_addr);

			/* register region as user of RAM dataspaces */
			{
				Object_pool<Dataspace_info>::Guard info(_ds_registry.lookup_info(ds));

				if (info) {
					info->register_user(*region);
				} else {
					if (verbose_attach) {
						PWRN("Trying to attach unknown dataspace type");
						PWRN("  ds_info@%p at 0x%lx size=%zd offset=0x%lx",
						     info.object(), (long)local_addr,
						     Dataspace_client(ds).size(), (long)offset);
					}
				}
			}

			/*
			 * Record attachment for later replay (needed during
			 * fork)
			 */
			Lock::Guard guard(_region_lock);
			_regions.insert(region);

			return local_addr;
		}

		void detach(Local_addr local_addr)
		{
			Region * region = 0;
			{
				Lock::Guard guard(_region_lock);
				region = _lookup_region_by_addr(local_addr);
				if (!region) {
					PWRN("Attempt to detach unknown region at 0x%p",
					     (void *)local_addr);
					return;
				}

				_regions.remove(region);
			}

			{
				Object_pool<Dataspace_info>::Guard info(_ds_registry.lookup_info(region->ds));
				if (info) info->unregister_user(*region);
			}

			destroy(env()->heap(), region);

			_rm.detach(local_addr);

		}

		Pager_capability add_client(Thread_capability thread)
		{
			return _rm.add_client(thread);
		}

		void remove_client(Pager_capability pager)
		{
			_rm.remove_client(pager);
		}

		void fault_handler(Signal_context_capability handler)
		{
			return _rm.fault_handler(handler);
		}

		State state()
		{
			return _rm.state();
		}

		Dataspace_capability dataspace()
		{
			return _rm.dataspace();
		}
};


inline void Noux::Rm_session_component::Region::dissolve(Dataspace_info &ds)
{
	rm.detach(local_addr);
}


#endif /* _NOUX__RM_SESSION_COMPONENT_H_ */
