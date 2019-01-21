/*
 * \brief  Timeout handling for netperf, based on test/alarm
 * \author Alexander Boettcher
 * \date   2014-01-10
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>

/* defined in "ports/contrib/netperf/src/netlib.c" */
extern "C" int times_up;


extern "C" void
start_timer(int time)
{
	Genode::warning(__func__, " not implemented, 'times_up' is never updated");
}
