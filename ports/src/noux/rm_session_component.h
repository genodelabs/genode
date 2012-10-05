/*
 * \brief  RM session implementation used by Noux processes
 * \author Norman Feske
 * \date   2012-02-22
 *
 * The custom RM implementation is used for recording all RM regions attached
 * to the region-manager session. Using the recorded information, the address-
 * space layout can then be replayed onto a new process created via fork.
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__RM_SESSION_COMPONENT_H_
#define _NOUX__RM_SESSION_COMPONENT_H_

/* Genode includes */
#include <rm_session/connection.h>
#include <base/rpc_server.h>

namespace Noux {

	class Rm_session_component : public Rpc_object<Rm_session>
	{
		private:

			/**
			 * Record of an attached dataspace
			 */
			struct Region : List<Region>::Element
			{
				Dataspace_capability ds;
				size_t               size;
				off_t                offset;
				addr_t               local_addr;

				Region(Dataspace_capability ds, size_t size,
				       off_t offset, addr_t local_addr)
				:
					ds(ds), size(size), offset(offset), local_addr(local_addr)
				{ }

				/**
				 * Return true if region contains specified address
				 */
				bool contains(addr_t addr) const
				{
					return (addr >= local_addr)
					    && (addr <  local_addr + size);
				}
			};

			List<Region> _regions;

			Region *_lookup_region_by_addr(addr_t local_addr)
			{
				Region *curr = _regions.first();
				for (; curr; curr = curr->next()) {
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

			Rm_session_component(Dataspace_registry &ds_registry,
			                     addr_t start = ~0UL, size_t size = 0)
			:
				_rm(start, size), _ds_registry(ds_registry)
			{ }

			~Rm_session_component()
			{
				Region *curr = 0;
				for (; curr; curr = curr->next())
					detach(curr->local_addr);
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
				for (Region *curr = _regions.first(); curr; curr = curr->next()) {

					Dataspace_capability ds;

					Dataspace_info *info = _ds_registry.lookup_info(curr->ds);

					if (info) {

						ds = info->fork(dst_ram, ds_registry, ep);

						/*
						 * XXX We could detect dataspaces that are attached
						 *     more than once. For now, we create a new fork
						 *     for each attachment.
						 */

					} else {

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

				Dataspace_info *info = _ds_registry.lookup_info(region->ds);
				if (!info) {
					PERR("attempt to write to unknown dataspace type");
					for (;;);
					return;
				}

				if (region->offset) {
					PERR("poke: writing to region with offset is not supported");
					return;
				}

				info->poke(dst_addr - region->local_addr, src, len);
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
				if (size == 0)
					size = Dataspace_client(ds).size();

				/*
				 * XXX look if we can identify the specified dataspace.
				 *     Is it a dataspace allocated via 'Local_ram_session'?
				 */

				local_addr = _rm.attach(ds, size, offset,
				                        use_local_addr, local_addr,
				                        executable);

				/*
				 * Record attachement for later replay (needed during
				 * fork)
				 */
				_regions.insert(new (env()->heap())
				                Region(ds, size, offset, local_addr));
				return local_addr;
			}

			void detach(Local_addr local_addr)
			{
				_rm.detach(local_addr);

				Region *region = _lookup_region_by_addr(local_addr);
				if (!region) {
					PWRN("Attempt to detach unknown region at 0x%p",
					     (void *)local_addr);
					return;
				}

				_regions.remove(region);
				destroy(env()->heap(), region);
			}

			Pager_capability add_client(Thread_capability thread)
			{
				return _rm.add_client(thread);
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
}

#endif /* _NOUX__RM_SESSION_COMPONENT_H_ */
