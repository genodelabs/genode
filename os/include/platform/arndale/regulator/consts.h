/*
 * \brief  Regulator definitions for Arndale
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2013-06-13
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PLATFORM__ARNDALE__REGULATOR__CONSTS_H_
#define _INCLUDE__PLATFORM__ARNDALE__REGULATOR__CONSTS_H_

#include <util/string.h>

namespace Regulator {

	enum Regulator_id {
		CLK_CPU,
		CLK_SATA,
		CLK_USB30,
		CLK_MMC0,
		PWR_SATA,
		PWR_USB30,
		MAX,
		INVALID
	};

	struct Regulator_name {
		Regulator_id id;
		const char * name;
	};

	Regulator_name names[] = {
		{ CLK_CPU,   "clock-cpu"    },
		{ CLK_SATA,  "clock-sata"   },
		{ CLK_USB30, "clock-usb3.0" },
		{ CLK_MMC0,  "clock-mmc0"   },
		{ PWR_SATA,  "power-sata"   },
		{ PWR_USB30, "power-usb3.0" },
	};

	Regulator_id regulator_id_by_name(const char * name)
	{
		for (unsigned i = 0; i < MAX; i++)
			if (Genode::strcmp(names[i].name, name) == 0)
				return names[i].id;
		return INVALID;
	}

	const char * regulator_name_by_id(Regulator_id id)   {
		return (id < MAX) ? names[id].name : 0; }


	/***************************************
	 ** Device specific level definitions **
	 ***************************************/

	enum Cpu_clock_freq {
		CPU_FREQ_200,
		CPU_FREQ_400,
		CPU_FREQ_600,
		CPU_FREQ_800,
		CPU_FREQ_1000,
		CPU_FREQ_1200,
		CPU_FREQ_1400,
		CPU_FREQ_1600,
		CPU_FREQ_1700, /* warning: 1700 not recommended by the reference manual
		              we just insert this for performance measurement against
		              Linux, which uses this overclocking */
		CPU_FREQ_MAX
	};
}

#endif /* _INCLUDE__PLATFORM__ARNDALE__REGULATOR__CONSTS_H_ */
