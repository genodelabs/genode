/*
 * \brief   Access to seL4 boot info
 * \author  Norman Feske
 * \date    2015-05-04
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <types.h>
#include <sel4_boot_info.h>

using namespace Core;


/* provided by the assembly startup code */
extern addr_t __initial_r0;

/**
 * Obtain seL4 boot info structure
 */
seL4_BootInfo const &Core::sel4_boot_info()
{
	return *(seL4_BootInfo const *)__initial_r0;
}
