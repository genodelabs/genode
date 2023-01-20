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

#ifndef _SRC__DRIVERS__PLATFORM__DEVICE_OWNER_H_
#define _SRC__DRIVERS__PLATFORM__DEVICE_OWNER_H_

namespace Driver
{
	struct Device;
	struct Device_owner;
}


struct Driver::Device_owner
{
	virtual void enable_device(Device const &)  {};
	virtual void disable_device(Device const &) {};
	virtual void update_devices_rom() {};
	virtual ~Device_owner() { }
};

#endif /* _SRC__DRIVERS__PLATFORM__DEVICE_OWNER_H_ */
