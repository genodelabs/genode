/*
 * \brief  Performance counter dummy
 * \author Josef Soentgen
 * \date   2013-09-26
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* base-hw includes */
#include <kernel/perf_counter.h>


void Kernel::Perf_counter::enable() { }


Kernel::Perf_counter* Kernel::perf_counter()
{
	static Kernel::Perf_counter inst;
	return &inst;
}
