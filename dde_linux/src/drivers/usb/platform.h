/**
 * \brief  Platform specific definitions
 * \author Sebastian Sumpf
 * \date   2012-07-06
 *
 * These functions have to be implemented on all supported platforms.
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

struct Services
{
	bool hid;
	bool stor;
	bool nic;

	Services() : hid(false), stor(false), nic(false) { }
};

void platform_hcd_init(Services *services);

#endif /* _PLATFORM_H_ */
