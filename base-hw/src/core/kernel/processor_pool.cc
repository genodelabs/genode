/*
 * \brief   Provide a processor object for every available processor
 * \author  Martin Stein
 * \date    2014-01-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/processor_pool.h>

using namespace Kernel;


Processor_pool * Kernel::processor_pool()
{
	static Processor_pool s;
	return &s;
}
