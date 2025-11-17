/*
 * \brief  Device_info stores GPU information
 * \author Sebastian Sumpf
 * \date   2025-03-11
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DEVICE_INFO_
#define _DEVICE_INFO_

#include <types.h>

namespace Igd {
	struct Device_info;
}

struct Igd::Device_info
{
	enum Platform { UNKNOWN, BROADWELL, SKYLAKE, KABYLAKE, WHISKEYLAKE, TIGERLAKE,
	                ALDERLAKE };
	enum Stepping { A0, B0, C0, D0, D1, E0, F0, G0 };

	uint16_t    id;
	uint8_t     generation;
	Platform    platform;
	uint64_t    features;
};

#endif /* _DEVICE_INFO_ */
