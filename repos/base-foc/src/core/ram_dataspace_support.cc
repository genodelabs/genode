/*
 * \brief  Export RAM dataspace as shared memory object (dummy)
 * \author Norman Feske
 * \date   2006-07-03
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <ram_dataspace_factory.h>
#include <map_local.h>

/* Fiasco.OC includes */
#include <foc/syscall.h>

using namespace Core;


bool Ram_dataspace_factory::_export_ram_ds(Dataspace_component &) { return true; }


void Ram_dataspace_factory::_revoke_ram_ds(Dataspace_component &) { }


void Ram_dataspace_factory::_clear_ds(Dataspace_component &ds)
{
	memset((void *)ds.phys_addr(), 0, ds.size());

	if (ds.cacheability() != CACHED)
		Foc::l4_cache_dma_coherent(ds.phys_addr(), ds.phys_addr() + ds.size());
}

