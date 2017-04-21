/*
 * \brief  Timestamp implementation for the Genode Timer
 * \author Martin Stein
 * \date   2016-11-04
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <trace/timestamp.h>
#include <os/timer.h>

using namespace Genode;


Trace::Timestamp Timer_time_source::_timestamp()
{
	return Trace::timestamp();
}
