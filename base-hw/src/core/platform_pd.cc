/*
 * \brief   Protection-domain facility
 * \author  Martin Stein
 * \date    2012-02-12
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <platform_pd.h>

using namespace Genode;

Platform_pd::~Platform_pd()
{
	_tlb->remove_region(platform()->vm_start(), platform()->vm_size());
	regain_ram_from_tlb(_tlb);
	if (Kernel::bin_pd(_id)) {
		PERR("failed to destruct protection domain at kernel");
	}
}

