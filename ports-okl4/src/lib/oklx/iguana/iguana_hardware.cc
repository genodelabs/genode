/*
 * \brief  Iguana API functions needed by OKLinux
 * \author Stefan Kalkowski
 * \date   2009-05-07
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>

extern "C" {
#include <iguana/hardware.h>


	int hardware_register_interrupt(L4_ThreadId_t thrd, int interrupt)
	{
		PWRN("Not yet implemented!");
		return 0;
	}


	int hardware_back_memsection(memsection_ref_t memsection,
	                             uintptr_t paddr, uintptr_t attributes)
	{
		PWRN("Not yet implemented!");
		return 0;
	}

} // extern "C"

