/*
 * \brief  Wrapper to update jiffies before invoking schedule_timeout().
 *         Schedule_timeout() expects that the jiffie value is current,
 *         in order to setup the timeouts. Without current jiffies,
 *         the programmed timeouts are too short, which leads to timeouts
 *         firing too early. The Intel driver uses this mechanism frequently
 *         by utilizing wait_queue_timeout*() in order to wait for hardware
 *         state changes, e.g. connectors hotplug. The schedule_timeout is
 *         shadowed by the Linker feature '--wrap'. This code can be removed
 *         as soon as the timeout handling is implemented by lx_emul/lx_kit
 *         instead of using the original Linux sources of kernel/time/timer.c.
 * \author Alexander Boettcher
 * \date   2022-04-04
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul/time.h>


signed long __real_schedule_timeout(signed long timeout);


signed long __wrap_schedule_timeout(signed long timeout)
{
	lx_emul_force_jiffies_update();
	return __real_schedule_timeout(timeout);
}
