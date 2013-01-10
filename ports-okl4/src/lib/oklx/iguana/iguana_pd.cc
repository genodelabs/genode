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

/* Genode includes */
#include <base/printf.h>

extern "C" {
#include <iguana/pd.h>


	pd_ref_t pd_myself(void)
	{
		PWRN("Not yet implemented!");
		return 0;
	}


	void pd_delete(pd_ref_t pd)
	{
		PWRN("Not yet implemented!");
	}


	memsection_ref_t pd_create_memsection(pd_ref_t pd, uintptr_t size,
	                                      uintptr_t *base)
	{
		PWRN("Not yet implemented!");
		return 0;
	}

} // extern "C"
