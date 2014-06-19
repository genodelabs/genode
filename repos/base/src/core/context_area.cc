/*
 * \brief  Support code for the thread API
 * \author Norman Feske
 * \date   2010-01-13
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <rm_session/rm_session.h>
#include <ram_session/ram_session.h>
#include <base/printf.h>
#include <base/thread.h>

/* local includes */
#include <platform.h>
#include <map_local.h>
#include <dataspace_component.h>

using namespace Genode;


/**
 * Pointer to dataspace used to hold core contexts
 */
enum { MAX_CORE_CONTEXTS = 256 };
static Dataspace_component *context_ds[MAX_CORE_CONTEXTS];


/**
 * Region-manager session for allocating thread contexts
 *
 * This class corresponds to the managed dataspace that is normally
 * used for organizing thread contexts with the thread context area.
 * It "emulates" the sub address space by adjusting the local address
 * argument to 'attach' with the offset of the thread context area.
 */
class Context_area_rm_session : public Rm_session
{
	enum { verbose = false };

	public:

		/**
		 * Attach backing store to thread-context area
		 */
		Local_addr attach(Dataspace_capability ds_cap,
		                  size_t size, off_t offset,
		                  bool use_local_addr, Local_addr local_addr,
		                  bool executable)
		{
			Dataspace_component *ds =
				dynamic_cast<Dataspace_component*>(Dataspace_capability::deref(ds_cap));
			if (!ds) {
				PERR("dataspace for core context does not exist");
				return (addr_t)0;
			}

			addr_t core_local_addr = Native_config::context_area_virtual_base() +
			                         (addr_t)local_addr;

			if (verbose)
				PDBG("core_local_addr = %lx, phys_addr = %lx, size = 0x%zx",
				     core_local_addr, ds->phys_addr(), ds->size());

			if (!map_local(ds->phys_addr(), core_local_addr,
			               ds->size() >> get_page_size_log2())) {
				PERR("could not map phys %lx at local %lx", ds->phys_addr(), core_local_addr);
				return (addr_t)0;
			}

			ds->assign_core_local_addr((void*)core_local_addr);

			return local_addr;
		}

		void detach(Local_addr local_addr)
		{
			addr_t core_local_addr = Native_config::context_area_virtual_base() +
			                         (addr_t)local_addr;

			Dataspace_component *ds = 0;

			/* find the dataspace component for the given address */
			for (unsigned i = 0; i < MAX_CORE_CONTEXTS; i++) {
				if (context_ds[i] &&
					(core_local_addr >= context_ds[i]->core_local_addr()) &&
				    (core_local_addr < (context_ds[i]->core_local_addr() +
				                        context_ds[i]->size()))) {
					ds = context_ds[i];
					break;
				}
			}

			if (!ds) {
				PERR("dataspace for core context does not exist");
				return;
			}

			if (verbose)
				PDBG("core_local_addr = %lx, phys_addr = %lx, size = 0x%zx",
				     ds->core_local_addr(), ds->phys_addr(), ds->size());

			Genode::unmap_local(ds->core_local_addr(), ds->size() >> get_page_size_log2());
		}

		Pager_capability add_client(Thread_capability) {
			return Pager_capability(); }

		void remove_client(Pager_capability) { }

		void fault_handler(Signal_context_capability) { }

		State state() { return State(); }

		Dataspace_capability dataspace() { return Dataspace_capability(); }
};


class Context_area_ram_session : public Ram_session
{
	private:

		enum { verbose = false };

		using Ds_slab = Synchronized_allocator<Tslab<Dataspace_component,
		                                             get_page_size()> >;

		Ds_slab _ds_slab { platform()->core_mem_alloc() };

	public:

		Ram_dataspace_capability alloc(size_t size, Cache_attribute cached)
		{
			/* find free context */
			unsigned i;
			for (i = 0; i < MAX_CORE_CONTEXTS; i++)
				if (!context_ds[i])
					break;

			if (i == MAX_CORE_CONTEXTS) {
				PERR("maximum number of core contexts (%d) reached", MAX_CORE_CONTEXTS);
				return Ram_dataspace_capability();
			}

			/* allocate physical memory */
			size = round_page(size);
			void *phys_base;
			if (platform_specific()->ram_alloc()->alloc_aligned(size, &phys_base,
			                                                    get_page_size_log2()).is_error()) {
				PERR("could not allocate backing store for new context");
				return Ram_dataspace_capability();
			}

			if (verbose)
				PDBG("phys_base = %p, size = 0x%zx", phys_base, size);

			context_ds[i] = new (&_ds_slab)
				Dataspace_component(size, 0, (addr_t)phys_base, CACHED, true, 0);

			Dataspace_capability cap = Dataspace_capability::local_cap(context_ds[i]);
			return static_cap_cast<Ram_dataspace>(cap);
		}

		void free(Ram_dataspace_capability ds)
		{
			Dataspace_component *dataspace_component =
				dynamic_cast<Dataspace_component*>(Dataspace_capability::deref(ds));

			if (!dataspace_component)
				return;

			for (unsigned i = 0; i < MAX_CORE_CONTEXTS; i++)
				if (context_ds[i] == dataspace_component) {
					context_ds[i] = 0;
					break;
				}

			void *phys_addr = (void*)dataspace_component->phys_addr();
			size_t size = dataspace_component->size();

			if (verbose)
				PDBG("phys_addr = %p, size = 0x%zx", phys_addr, size);

			destroy(&_ds_slab, dataspace_component);
			platform_specific()->ram_alloc()->free(phys_addr, size);
		}

		int ref_account(Ram_session_capability ram_session) { return 0; }

		int transfer_quota(Ram_session_capability ram_session, size_t amount) { return 0; }

		size_t quota() { return 0; }

		size_t used() { return 0; }
};


/**
 * Return single instance of the context-area RM and RAM session
 */
namespace Genode {

	Rm_session *env_context_area_rm_session()
	{
		static Context_area_rm_session inst;
		return &inst;
	}

	Ram_session *env_context_area_ram_session()
	{
		static Context_area_ram_session inst;
		return &inst;
	}
}

