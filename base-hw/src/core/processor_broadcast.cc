/*
 * \brief   Utility to execute a function on all available processors
 * \author  Martin Stein
 * \date    2014-03-07
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <processor_broadcast.h>

using namespace Genode;


Processor_broadcast * Genode::processor_broadcast()
{
	static Processor_broadcast s;
	return &s;
}
