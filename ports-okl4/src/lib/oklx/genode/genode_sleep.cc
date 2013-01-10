/*
 * \brief  Timer functions needed by Genode/OKLinux
 * \author Stefan Kalkowski
 * \date   2009-05-07
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <timer_session/connection.h>
#include <base/sleep.h>

/*
 * We only have one timer connection by now, as only the timer thread
 * in OKLinux uses this functionality
 */
static Timer::Connection timer;

extern "C"
{
#include <genode/sleep.h>


	void genode_sleep(unsigned ms) { timer.msleep(ms); }


	void genode_sleep_forever() { Genode::sleep_forever(); }

} // extern "C"
