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

#ifndef _CORE__INCLUDE__SEL4_BOOT_INFO_H_
#define _CORE__INCLUDE__SEL4_BOOT_INFO_H_

/* Genode includes */
#include <base/stdint.h>

/* seL4 includes */
#include <sel4/bootinfo.h>


/* provided by the assembly startup code */
extern Genode::addr_t __initial_bx;


namespace Genode {

	/**
	 * Obtain seL4 boot info structure
	 */
	static inline seL4_BootInfo const &sel4_boot_info()
	{
		return *(seL4_BootInfo const *)__initial_bx;
	}
}

#endif /* _CORE__INCLUDE__SEL4_BOOT_INFO_H_ */
