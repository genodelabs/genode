/*
 * \brief  Access to the kernel info page
 * \author Julian Stecklina
 * \date   2008-02-20
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core-local includes */
#include <kip.h>

using namespace Pistachio;

#include <l4/kip.h>


L4_KernelInterfacePage_t *Pistachio::get_kip()
{
	static L4_KernelInterfacePage_t *kip = 0;

	if (kip == 0)
		kip = reinterpret_cast<L4_KernelInterfacePage_t *>(L4_KernelInterface());

	return kip;
}


unsigned int Pistachio::get_page_size_log2()
{
	static unsigned int ps = 0;

	if (ps == 0) {
		L4_Word_t ps_mask = L4_PageSizeMask(get_kip());

		while ((ps_mask&1) == 0) {
			ps += 1;
			ps_mask >>= 1;
		}
	}
	return ps;
}


L4_Word_t Pistachio::get_page_mask()
{
	static L4_Word_t page_mask = 0;

	if (page_mask == 0) {
		unsigned int ps = get_page_size_log2();
		page_mask = (((L4_Word_t)~0)>>ps)<<ps;
	}
	return page_mask;
}
