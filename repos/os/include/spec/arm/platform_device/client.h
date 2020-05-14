/*
 * \brief  Client-side interface for ARM device
 * \author Stefan Kalkowski
 * \date   2020-04-15
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARM__PLATFORM_DEVICE__CLIENT_H_
#define _INCLUDE__SPEC__ARM__PLATFORM_DEVICE__CLIENT_H_

#include <platform_session/platform_session.h>
#include <platform_device/platform_device.h>
#include <base/rpc_client.h>
#include <io_mem_session/client.h>

namespace Platform { struct Device_client; }


struct Platform::Device_client : Genode::Rpc_client<Device>
{
	Device_client(Device_capability device)
	: Genode::Rpc_client<Device>(device) {}

	Genode::Irq_session_capability irq(unsigned id = 0) override
	{
		return call<Rpc_irq>(id);
	}

	Genode::Io_mem_session_capability
	io_mem(unsigned id = 0,
	       Genode::Cache_attribute caching =
	       Genode::Cache_attribute::UNCACHED) override
	{
		return call<Rpc_io_mem>(id, caching);
	}


	/***************************
	 ** Convenience utilities **
	 ***************************/

	Genode::Io_mem_dataspace_capability
	io_mem_dataspace(unsigned id = 0,
	                 Genode::Cache_attribute caching =
	                 Genode::Cache_attribute::UNCACHED)
	{
		Genode::Io_mem_session_client session(io_mem(id, caching));
			return session.dataspace();
	}
};

#endif /* _INCLUDE__SPEC__ARM__PLATFORM_DEVICE__CLIENT_H_ */
