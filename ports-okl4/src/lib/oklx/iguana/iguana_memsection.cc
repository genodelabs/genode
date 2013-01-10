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

#include <base/printf.h>
#include <oklx_memory_maps.h>

extern "C" {

	namespace Okl4 {
#include <stddef.h>
#include <l4/kdebug.h>
#include <iguana/memsection.h>
#include <iguana/thread.h>
	}

	using namespace Okl4;


	uintptr_t memsection_virt_to_phys(uintptr_t vaddr, size_t *size)
	{
		using namespace Genode;
		Memory_area *area = Memory_area::memory_map()->first();
		while(area)
		{
			if(area->vaddr() == vaddr)
			{
				uintptr_t paddr = area->paddr();
				*size = area->size();
				return paddr;
			}
			area = area->next();
		}
		PWRN("Memory area beginning @vaddr=0x%08lx doesn't exist!",
			 (unsigned long)vaddr);
		return 0;
	}


	int memsection_page_map(memsection_ref_t memsect, L4_Fpage_t from_page,
	                        L4_Fpage_t to_page)
	{
		PWRN("Not yet implemented!");
		return 0;
	}


	int memsection_page_unmap(memsection_ref_t memsect, L4_Fpage_t to_page)
	{
		PWRN("Not yet implemented!");
		return 0;
	}


	int memsection_register_server(memsection_ref_t memsect,
	                               thread_ref_t server)
	{
		PWRN("Not yet implemented!");
		return 0;
	}


	memsection_ref_t memsection_lookup(objref_t object, thread_ref_t *server)
	{
		return object;
	}


	void *memsection_base(memsection_ref_t memsect)
	{
		using namespace Genode;
		Memory_area *area = Memory_area::memory_map()->first();
		while(area)
		{
			if((area->vaddr() <= memsect) &&
			   ((area->vaddr()+area->size()) >= memsect))
				return (void*) area->vaddr();
			area = area->next();
		}
		PWRN("Memory area with @vaddr=0x%08lx doesn't exist!",
			 (unsigned long)memsect);
		return 0;
	}


	uintptr_t memsection_size(memsection_ref_t memsect)
	{
		using namespace Genode;
		Memory_area *area = Memory_area::memory_map()->first();
		while(area)
		{
			if((area->vaddr() <= memsect) &&
			   ((area->vaddr()+area->size()) >= memsect))
				return area->size();
			area = area->next();
		}
		PWRN("Memory area with @vaddr=0x%08lx doesn't exist!",
			 (unsigned long)memsect);
		return 0;
	}


	memsection_ref_t memsection_create_user(uintptr_t size, uintptr_t *base)
	{
		PWRN("Not yet implemented!");
		return 0;
	}


	void memsection_delete(memsection_ref_t)
	{
		PWRN("Not yet implemented!");
	}


	memsection_ref_t memsection_create(uintptr_t size, uintptr_t *base)
	{
		PWRN("Not yet implemented!");
		return 0;
	}

} // extern "C"

