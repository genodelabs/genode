/*
 * \brief  Iguana API functions needed by OKLinux
 * \author Stefan Kalkowski
 * \date   2009-05-07
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <dataspace/client.h>

/* OKLinux support includes */
#include <oklx_threads.h>
#include <oklx_memory_maps.h>

namespace Okl4 {

/* OKL4 includes */
#include <l4/types.h>
#include <l4/space.h>
}

using namespace Okl4;


/**
 * Get OKLinux exregs page, a special page used to synchronize
 * OKLinux kernel main thread and the user processes
 */
static Genode::addr_t get_exregs_page()
{
	using namespace Genode;

	static bool initialized = false;
	static addr_t phys_addr = 0;

	if(!initialized)
	{
		/* Order one page and copy the src content to it */
		extern char __user_exregs_page[];
		Dataspace_capability ds = env()->ram_session()->alloc(1 << 12);
		Dataspace_client dsc(ds);
		void* page = env()->rm_session()->attach(ds);
		memcpy(page, (const void*)&__user_exregs_page, 1 << 12);
		phys_addr = dsc.phys_addr();
	}
	return phys_addr;
}


extern "C" {


	L4_ThreadId_t eas_create_thread(eas_ref_t eas, L4_ThreadId_t pager,
	                                L4_ThreadId_t scheduler,
	                                void *utcb, L4_ThreadId_t *handle_rv)
	{
		using namespace Genode;

		/* Find the right user process and add a thread to it */
		Oklx_process *p = Oklx_process::processes()->first();
		while(p)
		{
			if(p->pd()->space_id().raw == eas)
				return p->add_thread();
			p = p->next();
		}
		PWRN("Oklinux user process %lx not found!", eas);
		return L4_nilthread;
	}


	eas_ref_t eas_create(L4_Fpage_t utcb, L4_SpaceId_t *l4_id)
	{
		using namespace Genode;

		/* Create a new OKLinux user process and return its space id */
		Oklx_process *p = 0;
		try {
			p = new (env()->heap()) Oklx_process();
			Oklx_process::processes()->insert(p);
			*l4_id = p->pd()->space_id();
			return l4_id->raw;
		}
		catch(...) {
			PWRN("Could not create a new protection domain!\n");
			if(p)
				destroy(env()->heap(),p);
			return 0;
		}
	}


	void eas_delete(eas_ref_t eas)
	{
		using namespace Genode;

		/* Find the process and remove it from the global list */
		Oklx_process *p = Oklx_process::processes()->first();
		while(p)
		{
			if(p->pd()->space_id().raw == eas)
			{
				Oklx_process::processes()->remove(p);
				destroy(env()->heap(), p);
				return;
			}
			p = p->next();
		}
	}


	int eas_map(eas_ref_t eas, L4_Fpage_t src_fpage, uintptr_t dst_addr,
	            uintptr_t attributes)
	{
		using namespace Genode;
		using namespace Okl4;

		unsigned long dest_addr = dst_addr & 0xfffff000;
		unsigned long phys_addr = 0;
		unsigned long src_addr  = L4_Address(src_fpage);

		/* Dirty hack for the evil oklx exregs page */
		if(dest_addr == 0x98765000UL)
			phys_addr = get_exregs_page();
		else
		{
			Memory_area *ma = Memory_area::memory_map()->first();
			while(ma)
			{
				if(src_addr >= ma->vaddr() &&
				   src_addr < ma->vaddr()+ma->size())
				{
					phys_addr = ma->paddr() + src_addr - ma->vaddr();
					break;
				}
				ma = ma->next();
			}
		}
		if(phys_addr == 0)
			PERR("wants to map from=0x%08lx to=0x%08lx", src_addr, dest_addr);
		L4_Fpage_t vpage = L4_Fpage(dest_addr, 1 << 12);
		L4_PhysDesc_t pdesc = L4_PhysDesc(phys_addr, attributes);
		int rwx = L4_Rights(src_fpage);
		L4_SpaceId_t id;
		id.raw = eas;
		L4_FpageAddRightsTo(&vpage, rwx);
		if(!L4_MapFpage(id, vpage, pdesc))
			PERR("Mapping failed");
		return 0;
	}

} // extern "C"
