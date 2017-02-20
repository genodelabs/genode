/*
 * \brief  Regulator definitions for Exynos5
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2013-06-13
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__EXYNOS5__REGULATOR__CONSTS_H_
#define _INCLUDE__SPEC__EXYNOS5__REGULATOR__CONSTS_H_

#include <util/string.h>

namespace Regulator {

	enum Regulator_id {
		CLK_CPU,
		CLK_SATA,
		CLK_USB30,
		CLK_USB20,
		CLK_MMC0,
		CLK_HDMI,
		PWR_SATA,
		PWR_USB30,
		PWR_USB20,
		PWR_HDMI,
		MAX,
		INVALID
	};

	struct Regulator_name {
		Regulator_id id;
		const char * name;
	};

	static constexpr Regulator_name names[] = {
		{ CLK_CPU,   "clock-cpu"     },
		{ CLK_SATA,  "clock-sata"    },
		{ CLK_USB30, "clock-usb3.0"  },
		{ CLK_USB20, "clock-usb2.0"  },
		{ CLK_MMC0,  "clock-mmc0"    },
		{ CLK_HDMI,  "clock-hdmi"    },
		{ PWR_SATA,  "power-sata"    },
		{ PWR_USB30, "power-usb3.0"  },
		{ PWR_USB20, "power-usb2.0"  },
		{ PWR_HDMI,  "power-hdmi"},
	};

	inline Regulator_id regulator_id_by_name(const char * name)
	{
		for (unsigned i = 0; i < sizeof(names)/sizeof(names[0]); i++)
			if (Genode::strcmp(names[i].name, name) == 0)
				return names[i].id;
		return INVALID;
	}

	inline const char * regulator_name_by_id(Regulator_id id)   {
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
		CPU_FREQ_1600 = 1600000000,
		CPU_FREQ_1700 = 1700000000,
		/* warning: 1700 not recommended by the reference manual
		   we just insert this for performance measurement against
		   Linux, which uses this overclocking */
	};
}

#endif /* _INCLUDE__SPEC__EXYNOS5__REGULATOR__CONSTS_H_ */
