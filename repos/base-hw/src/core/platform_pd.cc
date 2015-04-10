/*
 * \brief   Protection-domain facility
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-02-12
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <platform_pd.h>

using namespace Genode;

void Platform_pd::flush(addr_t virt_base, size_t size)
{
	Lock::Guard guard(*lock());

	if (_tt) _tt->remove_translation(virt_base, size, page_slab());

	/* update translation caches */
	Kernel::update_pd(kernel_pd());
}


Platform_pd::~Platform_pd()
{
	Lock::Guard guard(_lock);

	Kernel::delete_pd(_kernel_pd);
	_tt->remove_translation(platform()->vm_start(), platform()->vm_size(),
	                        _pslab);

	/* TODO: destroy page slab and translation table!!! */
}
