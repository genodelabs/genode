/*
 * \brief  Kernel backend for virtual machines
 * \author Martin Stein
 * \date   2013-09-15
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/vm.h>

namespace Kernel
{
	Vm_ids * vm_ids() { return unsynchronized_singleton<Vm_ids>(); }
	Vm_pool * vm_pool() { return unsynchronized_singleton<Vm_pool>(); }
}
