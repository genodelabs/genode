/*
 * \brief   Protection-domain facility
 * \author  Norman Feske
 * \date    2015-05-01
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform_pd.h>

#include "arch_kernel_object.h"

Genode::addr_t Genode::Platform_pd::_init_page_directory()
{
	addr_t const phys_addr = Untyped_memory::alloc_page(*platform()->ram_alloc());
	seL4_Untyped const service = Untyped_memory::untyped_sel(phys_addr).value();

	create<Page_directory_kobj>(service,
	                            platform_specific()->core_cnode().sel(),
	                            _page_directory_sel);

	long ret = seL4_X86_ASIDPool_Assign(platform_specific()->asid_pool().value(),
	                                    _page_directory_sel.value());

	if (ret != seL4_NoError)
		error("seL4_X86_ASIDPool_Assign returned ", ret);

	return phys_addr;
}
