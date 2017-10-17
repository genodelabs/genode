/*
 * \brief   Virtual-memory space
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
#include <vm_space.h>

long Genode::Vm_space::_map_page(Genode::Cap_sel const &idx,
                                 Genode::addr_t const virt,
                                 Cache_attribute const cacheability,
                                 bool            const writable,
                                 bool            const executable)
{
	seL4_X86_Page          const service = _idx_to_sel(idx.value());
	seL4_X86_PageDirectory const pd      = _pd_sel.value();
	seL4_CapRights_t       const rights  = writable ? seL4_ReadWrite : seL4_CanRead;
	seL4_X86_VMAttributes        attr    = seL4_X86_Default_VMAttributes;

	if (cacheability == UNCACHED)
		attr = seL4_X86_Uncacheable;
	else
	if (cacheability == WRITE_COMBINED)
		attr = seL4_X86_WriteCombining;

	return seL4_X86_Page_Map(service, pd, virt, rights, attr);
}

long Genode::Vm_space::_unmap_page(Genode::Cap_sel const &idx)
{
	seL4_X86_Page const service = _idx_to_sel(idx.value());
	return seL4_X86_Page_Unmap(service);
}
