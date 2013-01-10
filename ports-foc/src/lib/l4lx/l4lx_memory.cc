/*
 * \brief  L4lxapi library memory functions.
 * \author Stefan Kalkowski
 * \date   2011-04-11
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

#include <l4lx_memory.h>
#include <env.h>

namespace Fiasco {
#include <l4/sys/consts.h>
}

extern "C" {

void l4lx_memory_map_physical_page(unsigned long page)
{
	void* phys = (void*)Fiasco::l4_trunc_page(page);
	L4lx::Env::env()->rm()->map(phys);
}


/**
 * \brief Map a page into the virtual address space.
 * \ingroup memory
 *
 * \param	address		Virtual address.
 * \param	page		Physical address.
 * \param       map_rw          True if map should be mapped rw.
 *
 * \return	0 on success, != 0 on error
 */
int l4lx_memory_map_virtual_page(unsigned long address, unsigned long page,
                                 int map_rw)
{
	Linux::Irq_guard guard;

	void* phys = (void*)Fiasco::l4_trunc_page(page);
	void* virt = (void*)Fiasco::l4_trunc_page(address);
	L4lx::Env::env()->rm()->add_mapping(phys, virt, map_rw);
	L4lx::Env::env()->rm()->map(phys);
	return 0;
}

/**
 * \brief Unmap a page from the virtual address space.
 * \ingroup memory
 *
 * \param	address		Virtual adress.
 *
 * \return	0 on success, != 0 on error
 */
int l4lx_memory_unmap_virtual_page(unsigned long address)
{
	Linux::Irq_guard guard;

	L4lx::Env::env()->rm()->remove_mapping((void*)Fiasco::l4_trunc_page(address));
	return 0;
}

/**
 * \brief Return if something is mapped at given address
 * \ingroup memory
 *
 * \param	address		Address to query
 *
 * \return	1 if a page is mapped, 0 if not
 */
int l4lx_memory_page_mapped(unsigned long address)
{
	Linux::Irq_guard guard;

	return (L4lx::Env::env()->rm()->phys((void*)Fiasco::l4_trunc_page(address))) ? 1 : 0;
}

}
