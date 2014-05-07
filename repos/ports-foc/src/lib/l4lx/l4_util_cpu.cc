/*
 * \brief  L4Re functions needed by L4Linux.
 * \author Stefan Kalkowski
 * \date   2011-04-17
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>

namespace Fiasco {
#include <l4/util/cpu.h>
}

using namespace Fiasco;

extern "C" {

	unsigned int l4util_cpu_capabilities(void)
	{
		PWRN("%s: Not implemented yet!",__func__);
		return 0;
	}

}
