/*
 * \brief   Platform thread interface implementation - x86 specific
 * \author  Alexander Boettcher
 * \date    2017-08-08
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <platform_thread.h>

void Genode::Platform_thread::affinity(Affinity::Location location)
{
	_location = location;
	seL4_TCB_SetAffinity(tcb_sel().value(), location.xpos());
}
