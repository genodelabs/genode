/*
 * \brief  Platform driver - Device owner abstraction
 * \author Johannes Schlatow
 * \date   2023-01-20
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVER__PLATFORM__DEVICE_OWNER_H_
#define _SRC__DRIVER__PLATFORM__DEVICE_OWNER_H_

#include <util/interface.h>

namespace Driver
{
	struct Device;
	struct Device_owner;
}


struct Driver::Device_owner : Genode::Interface
{
	virtual void enable_device(Device const &)  = 0;
	virtual void disable_device(Device const &) = 0;
};

#endif /* _SRC__DRIVER__PLATFORM__DEVICE_OWNER_H_ */
