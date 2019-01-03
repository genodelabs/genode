/*
 * \brief  Timestamp implementation for the Genode Timer
 * \author Martin Stein
 * \date   2016-11-04
 *
 * On ARM, we do not have a component-local hardware time-source. The ARM
 * performance counter has no reliable frequency as the ARM idle command
 * halts the counter. However, on the HW kernel, we use a syscall that reads
 * out the kernel time instead.
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <kernel/interface.h>
#include <timer_session/connection.h>

using namespace Genode;


Trace::Timestamp Timer::Connection::_timestamp()
{
	return Kernel::time();
}
