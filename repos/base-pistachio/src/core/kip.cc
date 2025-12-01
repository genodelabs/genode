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

/* core includes */
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
