/*
 * \brief  Client stub for Platform::Device client
 * \author Norman Feske
 * \date   2021-04-15
 *
 * The 'Device_client' is a mere compatibilty wrapper used until the
 * driver has been coverted to the 'Platform::Device::Mmio' API.
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PLATFORM_DEVICE_H_
#define _PLATFORM_DEVICE_H_

namespace Platform { struct Device_client; }


struct Platform::Device_client : Rpc_client<Device_interface>
{
	Device_client(Capability<Device_interface> cap)
	: Rpc_client<Device_interface>(cap) { }

	Irq_session_capability irq(unsigned id = 0)
	{
		return call<Rpc_irq>(id);
	}

	Io_mem_session_capability io_mem(unsigned id, Range &range, Cache cache)
	{
		return call<Rpc_io_mem>(id, range, cache);
	}

	Io_mem_dataspace_capability io_mem_dataspace(unsigned id = 0)
	{
		Range range { };
		return Io_mem_session_client(io_mem(id, range, UNCACHED)).dataspace();
	}
};

#endif /* _PLATFORM_DEVICE_H_ */
