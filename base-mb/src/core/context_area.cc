/*
 * \brief  Support code for the thread API
 * \author Norman Feske
 * \date   2010-01-13
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
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
	public:

		/**
		 * Attach backing store to thread-context area
		 */
		Local_addr attach(Dataspace_capability ds_cap,
		                  size_t size, off_t offset,
		                  bool use_local_addr, Local_addr local_addr)
		{
			Dataspace_component *ds = context_ds[ds_cap.local_name()];
			if (!ds) {
				PERR("dataspace for core context does not exist");
				return 0;
			}

			if (!map_local(ds->phys_addr(),
			               (addr_t)local_addr + Thread_base::CONTEXT_AREA_VIRTUAL_BASE,
			               ds->size() >> get_page_size_log2()))
				return 0;

			return local_addr;
		}

		void detach(Local_addr) { }

		Pager_capability add_client(Thread_capability) {
			return Pager_capability(); }

		void fault_handler(Signal_context_capability) { }

		State state() { return State(); }

		Dataspace_capability dataspace() { return Dataspace_capability(); }
};


class Context_area_ram_session : public Ram_session
{
	public:

		Ram_dataspace_capability alloc(size_t size, bool)
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
			if (!platform_specific()->ram_alloc()->alloc_aligned(size, &phys_base,
			                                                     get_page_size_log2())) {
				PERR("could not allocate backing store for new context");
				return Ram_dataspace_capability();
			}

			context_ds[i] = new (platform()->core_mem_alloc())
				Dataspace_component(size, 0, (addr_t)phys_base, false, true);

			/*
			 * We do not manage the dataspace via an entrypoint because it will
			 * only be used by the 'context_area_rm_session'. Therefore, we
			 * construct a "capability" by hand using the context ID as local
			 * name.
			 */
			Native_capability cap;
			return reinterpret_cap_cast<Ram_dataspace>(Native_capability(cap.dst(), i));
		}

		void free(Ram_dataspace_capability ds) { PDBG("not yet implemented"); }

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
