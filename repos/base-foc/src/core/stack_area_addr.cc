/*
 * \brief  Component-local stack area base address for core on Fiasco.OC
 * \author Stefan Kalkowski
 * \date   2017-06-02
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base-internal includes */
#include <base/internal/stack_area.h>

/*
 * The base address of the context area differs for core, because
 * roottask on Fiasco.OC uses identity mappings. The virtual address range
 * of the stack area must not overlap with physical memory. We pick an
 * address range that lies outside of the RAM of the currently supported
 * platforms.
 */
Genode::addr_t Genode::stack_area_virtual_base() { return 0x20000000UL; }
