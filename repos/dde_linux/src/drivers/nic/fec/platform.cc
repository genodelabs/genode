/*
 * \brief  Platform driver relevant lx_kit backend functions
 * \author Stefan Kalkowski
 * \date   2017-11-01
 *
 * Taken from the USB driver.
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/ram_allocator.h>

#include <legacy/lx_kit/backend_alloc.h>
#include <legacy/lx_kit/env.h>


/****************************
 ** lx_kit/backend_alloc.h **
 ****************************/

void backend_alloc_init(Genode::Env&, Genode::Ram_allocator&,
						Genode::Allocator&)
{
	/* intentionally left blank */
}


Genode::Ram_dataspace_capability
Lx::backend_alloc(Genode::addr_t size, Genode::Cache cache) {
	return Lx_kit::env().env().ram().alloc(size, cache); }


void Lx::backend_free(Genode::Ram_dataspace_capability cap) {
	return Lx_kit::env().env().ram().free(cap); }
