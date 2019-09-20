/*
 * \brief  Globally available accessors to Libc kernel functionality
 * \author Norman Feske
 * \date   2019-09-18
 *
 * XXX  eliminate the need for these functions, or
 *      turn them into regular members of 'Libc::Kernel'
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* libc-internal includes */
#include <internal/legacy.h>
#include <internal/kernel.h>


void Libc::dispatch_pending_io_signals()
{
	Kernel::kernel().dispatch_pending_io_signals();
}


void Libc::schedule_suspend(void (*suspended) ())
{
	Kernel::kernel().schedule_suspend(suspended);
}
