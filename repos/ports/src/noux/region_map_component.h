/*
 * \brief  Region map implementation used by Noux processes
 * \author Norman Feske
 * \author Martin Stein
 * \date   2012-02-22
 *
 * The custom region-map implementation is used for recording all regions
 * attached to the region map. Using the recorded information, the address-
 * space layout can then be replayed onto a new process created via fork.
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOUX__REGION_MAP_COMPONENT_H_
#define _NOUX__REGION_MAP_COMPONENT_H_

/* Genode includes */
#include <region_map/client.h>
#include <base/rpc_server.h>
#include <util/retry.h>
#include <pd_session/capability.h>

/* Noux includes */
#include <dataspace_registry.h>

namespace Noux { class Region_map_component; }


class Noux::Region_map_component : public Rpc_object<Region_map>,
                                   public Dataspace_info
{
	private:

		static constexpr bool verbose_attach = false;
		static constexpr bool verbose_replay = false;

		Allocator &_alloc;

		Rpc_entrypoint &_ep;

		/**
		 * Record of an attached dataspace
		 */
		struct Region : List<Region>::Element, Dataspace_user
		{
			Region_map_component &rm;
			Dataspace_capability  ds;
			size_t                size;
			off_t                 offset;
			addr_t                local_addr;

			Region(Region_map_component &rm,
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
		 * Wrapped region map at core
		 */
		Region_map_client _rm;

		Pd_connection &_pd;

		Dataspace_registry &_ds_registry;

	public:

		/**
		 * Constructor
		 *
		 * \param pd  protection domain the region map belongs to, used for
		 *            quota upgrades
		 * \param rm  region map at core
		 */
		Region_map_component(Allocator &alloc, Rpc_entrypoint &ep,
		                     Dataspace_registry &ds_registry,
		                     Pd_connection &pd,
		                     Capability<Region_map> rm)
		:
			Dataspace_info(Region_map_client(rm).dataspace()),
			_alloc(alloc), _ep(ep), _rm(rm), _pd(pd), _ds_registry(ds_registry)
		{
			_ep.manage(this);
			_ds_registry.insert(this);
		}

		/**
		 * Destructor
		 */
		~Region_map_component()
		{
			_ds_registry.remove(this);
			_ep.dissolve(this);

			Region *curr;
			while ((curr = _regions.first()))
				detach(curr->local_addr);
		}

		/**
		 * Return address where the specified dataspace is attached
		 *
		 * This function is used by the 'Pd_session_component' to look up
		 * the base addresses for the stack area and linker area.
		 */
		addr_t lookup_region_base(Dataspace_capability ds)
		{
			Lock::Guard guard(_region_lock);

			Region *curr = _regions.first();
			for (; curr; curr = curr->next_region()) {
				if (curr->ds.local_name() == ds.local_name())
					return curr->local_addr;
			}
			return 0;
		}

		/**
		 * Replay attachments onto specified region map
		 *
		 * \param dst_ram      backing store used for allocating the
		 *                     the copies of RAM dataspaces
		 * \param ds_registry  dataspace registry used for keeping track
		 *                     of newly created dataspaces
		 * \param ep           entrypoint used to serve the RPC interface
		 *                     of forked managed dataspaces
		 */
		void replay(Ram_allocator      &dst_ram,
		            Region_map         &dst_rm,
		            Region_map         &local_rm,
		            Allocator          &alloc,
		            Dataspace_registry &ds_registry,
		            Rpc_entrypoint     &ep)
		{
			Lock::Guard guard(_region_lock);
			for (Region *curr = _regions.first(); curr; curr = curr->next_region()) {

				auto lambda = [&] (Dataspace_info *info)
				{
					Dataspace_capability ds;
					if (info) {

						ds = info->fork(dst_ram, local_rm, alloc, ds_registry, ep);

						/*
						 * XXX We could detect dataspaces that are attached
						 *     more than once. For now, we create a new fork
						 *     for each attachment.
						 */

					} else {

						warning("replay: missing ds_info for dataspace at addr ",
						        Hex(curr->local_addr));

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

					/*
					 * The call of 'info->fork' returns a invalid dataspace
					 * capability for the stack area and linker area. Those
					 * region maps are directly replayed and attached in
					 * 'Pd_session_component::replay'. So we can skip them
					 * here.
					 */
					if (!ds.valid()) {
						if (verbose_replay)
							warning("replay: skip dataspace of region ",
							        Hex(curr->local_addr));
						return;
					}

					dst_rm.attach(ds, curr->size, curr->offset, true, curr->local_addr);
				};
				_ds_registry.apply(curr->ds, lambda);
			};
		}


		/**************************
		 ** Region_map interface **
		 **************************/

		Local_addr attach(Dataspace_capability ds,
		                  size_t size = 0, off_t offset = 0,
		                  bool use_local_addr = false,
		                  Local_addr local_addr = (addr_t)0,
		                  bool executable = false) override
		{
			/*
			 * Region map subtracts offset from size if size is 0
			 */
			if (size == 0) size = Dataspace_client(ds).size() - offset;

			for (;;) {
				try {
					local_addr = _rm.attach(ds, size, offset, use_local_addr,
					                        local_addr, executable);
					break;
				}
				catch (Out_of_ram)  { _pd.upgrade_ram(8*1024); }
				catch (Out_of_caps) { _pd.upgrade_caps(2); }
			}

			Region * region = new (_alloc)
			                  Region(*this, ds, size, offset, local_addr);

			/* register region as user of RAM dataspaces */
			auto lambda = [&] (Dataspace_info *info)
			{
				if (info) {
					info->register_user(*region);
				} else {
					if (verbose_attach) {
						warning("trying to attach unknown dataspace type "
						        "ds=",         ds.local_name(), " "
						        "info@",       info,            " "
						        "local_addr=", Hex(local_addr), " "
						        "size=",       Dataspace_client(ds).size(), " "
						        "offset=",     Hex(offset));
					}
				}
			};
			_ds_registry.apply(ds, lambda);

			/*
			 * Record attachment for later replay (needed during fork)
			 */
			Lock::Guard guard(_region_lock);
			_regions.insert(region);

			return local_addr;
		}

		void detach(Local_addr local_addr) override
		{
			Region * region = 0;
			{
				Lock::Guard guard(_region_lock);
				region = _lookup_region_by_addr(local_addr);
				if (!region) {
					warning("attempt to detach unknown region at ", (void *)local_addr);
					return;
				}

				_regions.remove(region);
			}

			_ds_registry.apply(region->ds, [&] (Dataspace_info *info) {
				if (info) info->unregister_user(*region); });

			destroy(_alloc, region);

			_rm.detach(local_addr);
		}

		void fault_handler(Signal_context_capability handler) override
		{
			return _rm.fault_handler(handler);
		}

		State state() override
		{
			return _rm.state();
		}

		Dataspace_capability dataspace() override
		{
			/*
			 * We cannot call '_rm.dataspace()' here because NOVA would
			 * hand out a capability that is unequal to the one we got
			 * during the construction of the 'Dataspace_info' base class.
			 * To work around this problem, we return the capability
			 * that is kept in the 'Dataspace_info'.
			 */
			return ds_cap();
		}


		/******************************
		 ** Dataspace_info interface **
		 ******************************/

		Dataspace_capability fork(Ram_allocator      &,
		                          Region_map         &,
		                          Allocator          &,
		                          Dataspace_registry &,
		                          Rpc_entrypoint     &) override
		{
			return Dataspace_capability();
		}

		/**
		 * Return leaf region map that covers a given address
		 *
		 * \param addr  address that is covered by the requested region map
		 */
		Capability<Region_map> lookup_region_map(addr_t const addr) override
		{
			/* if there's no region that could be a sub RM then we're a leaf */
			Region * const region = _lookup_region_by_addr(addr);
			if (!region) { return Rpc_object<Region_map>::cap(); }

			auto lambda = [&] (Dataspace_info *info)
			{
				/* if there is no info for the region it can't be a sub RM */
				if (!info) { return Rpc_object<Region_map>::cap(); }

				/* ask the dataspace info for an appropriate sub RM */
				addr_t const region_base = region->local_addr;
				addr_t const region_off = region->offset;
				addr_t const sub_addr = addr - region_base + region_off;
				Capability<Region_map> sub_rm = info->lookup_region_map(sub_addr);

				/* if the result is invalid the dataspace is no sub RM */
				if (!sub_rm.valid()) { return Rpc_object<Region_map>::cap(); }
				return sub_rm;
			};
			return _ds_registry.apply(region->ds, lambda);
		}

		void poke(Region_map &rm, addr_t dst_addr, char const *src, size_t len) override
		{
			Dataspace_capability ds_cap;
			addr_t               local_addr;

			{
				Lock::Guard guard(_region_lock);

				Region *region = _lookup_region_by_addr(dst_addr);
				if (!region) {
					error("poke: no region at ", Hex(dst_addr));
					return;
				}

				/*
				 * Test if start and end address occupied by the object
				 * type refers to the same region.
				 */
				if (region != _lookup_region_by_addr(dst_addr + len - 1)) {
					error("attempt to write beyond region boundary");
					return;
				}

				if (region->offset) {
					error("poke: writing to region with offset is not supported");
					return;
				}

				ds_cap = region->ds;
				local_addr = region->local_addr;
			}

			_ds_registry.apply(ds_cap, [&] (Dataspace_info *info) {
				if (!info) {
					error("attempt to write to unknown dataspace type");
					for (;;);
				}
				info->poke(rm, dst_addr - local_addr, src, len);
			});
		}
};


inline void Noux::Region_map_component::Region::dissolve(Dataspace_info &ds)
{
	rm.detach(local_addr);
}


#endif /* _NOUX__REGION_MAP_COMPONENT_H_ */
