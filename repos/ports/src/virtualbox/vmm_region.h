/*
 * \brief  Memory region types
 * \author Norman Feske
 * \author Adrian-Ken Rueegsegger
 * \author Reto Buerki
 * \date   2013-09-02
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _VIRTUALBOX__VMM_REGION_H_
#define _VIRTUALBOX__VMM_REGION_H_

/* Genode includes */
#include <util/list.h>
#include <rm_session/connection.h>
#include <region_map/client.h>

/* VirtualBox includes */
#include <VBox/vmm/pgm.h>

struct Mem_region;

struct Mem_region : public  Genode::List<Mem_region>::Element,
					private Genode::Rm_connection,
					public  Genode::Region_map_client
{
	PPDMDEVINS           pDevIns;
	unsigned const       iRegion;
	RTGCPHYS             vm_phys;
	PFNPGMR3PHYSHANDLER  pfnHandlerR3;
	void                *pvUserR3;
	PGMPHYSHANDLERTYPE   enmType;

	Genode::addr_t       _base;
	Genode::size_t       _size;

	Mem_region(Genode::Env &env, size_t size,
	           PPDMDEVINS pDevIns, unsigned iRegion,
	           unsigned sub_rm_max_ds = 32 * 1024 * 1024)
	:
		Rm_connection(env),
		Region_map_client(Rm_connection::create(size)),
		pDevIns(pDevIns),
		iRegion(iRegion),
		vm_phys(0), pfnHandlerR3(0), pvUserR3(0),
		_base(env.rm().attach(Region_map_client::dataspace())),
		_size(size)

	{
		Genode::addr_t rest_size = _size;
		Genode::addr_t map_size  = rest_size < sub_rm_max_ds ? rest_size : sub_rm_max_ds;

		do {
			Genode::Ram_dataspace_capability ds = env.ram().alloc(map_size);
			attach_at(ds, _size - rest_size, map_size);

			rest_size -= map_size;
			map_size   = rest_size < sub_rm_max_ds ? rest_size : sub_rm_max_ds;
		} while (rest_size);
	}

	size_t size() { return _size; }

	template <typename T>
	T * local_addr() { return reinterpret_cast<T *>(_base); }
};

#endif /* _VIRTUALBOX__ACCLOFF__MEM_REGION_H_ */
