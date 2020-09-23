/*
 * \brief   Platform thread interface implementation - arm specific
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

void Genode::Platform_thread::affinity(Affinity::Location const location)
{
#if CONFIG_MAX_NUM_NODES > 1
	seL4_Error const res = seL4_TCB_SetAffinity(tcb_sel().value(), location.xpos());
	if (res == seL4_NoError)
		_location = location;
#else
	(void)location;
#endif
}
