/*
 * \brief  Regulator definitions for Exynos4412
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2013-06-13
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__EXYNOS4__REGULATOR__CONSTS_H_
#define _INCLUDE__SPEC__EXYNOS4__REGULATOR__CONSTS_H_

#include <util/string.h>

namespace Regulator {

	enum Regulator_id {
		CLK_CPU,
		CLK_USB20,
		CLK_MMC0,
		CLK_HDMI,
		PWR_USB20,
		PWR_HDMI,
		MAX,
		INVALID
	};

	struct Regulator_name {
		Regulator_id id;
		const char * name;
	};

	Regulator_name names[] = {
		{ CLK_CPU,   "clock-cpu"     },
		{ CLK_USB20, "clock-usb2.0"  },
		{ CLK_MMC0,  "clock-mmc0"    },
		{ CLK_HDMI,  "clock-hdmi"    },
		{ PWR_USB20, "power-usb2.0"  },
		{ PWR_HDMI,  "power-hdmi"},
	};

	Regulator_id regulator_id_by_name(const char * name)
	{
		for (unsigned i = 0; i < sizeof(names)/sizeof(names[0]); i++)
			if (Genode::strcmp(names[i].name, name) == 0)
				return names[i].id;
		return INVALID;
	}

	const char * regulator_name_by_id(Regulator_id id)   {
		return (id < sizeof(names)/sizeof(names[0])) ? names[id].name : 0; }


	/***************************************
	 ** Device specific level definitions **
	 ***************************************/

	enum Cpu_clock_freq {
		CPU_FREQ_200  = 200000000,
		CPU_FREQ_400  = 400000000,
		CPU_FREQ_600  = 600000000,
		CPU_FREQ_800  = 800000000,
		CPU_FREQ_1000 = 1000000000,
		CPU_FREQ_1200 = 1200000000,
		CPU_FREQ_1400 = 1400000000,
	};
}

#endif /* _INCLUDE__SPEC__EXYNOS4__REGULATOR__CONSTS_H_ */
