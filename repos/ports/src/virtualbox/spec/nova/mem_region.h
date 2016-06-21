/*
 * \brief  Memory region types
 * \author Norman Feske
 * \author Adrian-Ken Rueegsegger
 * \author Reto Buerki
 * \date   2013-09-02
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _VIRTUALBOX__SPEC__NOVA__MEM_REGION_H_
#define _VIRTUALBOX__SPEC__NOVA__MEM_REGION_H_

/* Genode includes */
#include <util/list.h>
#include <os/attached_ram_dataspace.h>


/* VirtualBox includes */
#include <VBox/vmm/pgm.h>

struct Mem_region;

struct Mem_region : Genode::List<Mem_region>::Element,
					Genode::Attached_ram_dataspace
{
	typedef Genode::Ram_session Ram_session;

	PPDMDEVINS           pDevIns;
	unsigned const       iRegion;
	RTGCPHYS             vm_phys;
	PFNPGMR3PHYSHANDLER  pfnHandlerR3;
	void                *pvUserR3;
	PGMPHYSHANDLERTYPE   enmType;

	Mem_region(Ram_session &ram, size_t size, PPDMDEVINS pDevIns,
	           unsigned iRegion)
	:
		Attached_ram_dataspace(&ram, size),
		pDevIns(pDevIns),
		iRegion(iRegion),
		vm_phys(0), pfnHandlerR3(0), pvUserR3(0)
	{ }
};

#endif /* _VIRTUALBOX__SPEC__NOVA__MEM_REGION_H_ */
