/*
 * \brief  Platform session interface
 * \author Stefan Kalkowski
 * \date   2020-04-28
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARM__PLATFORM_SESSION__PLATFORM_SESSION_H_
#define _INCLUDE__SPEC__ARM__PLATFORM_SESSION__PLATFORM_SESSION_H_

#include <base/quota_guard.h>
#include <base/rpc_args.h>
#include <dataspace/capability.h>
#include <platform_device/capability.h>
#include <platform_device/platform_device.h>
#include <rom_session/capability.h>
#include <session/session.h>

namespace Platform {
	using namespace Genode;

	struct Session;
}


struct Platform::Session : Genode::Session
{
	/*********************
	 ** Exception types **
	 *********************/

	class Fatal : public Out_of_ram { };

	/**
	 * \noapi
	 */
	static const char *service_name() { return "Platform"; }

	enum { RAM_QUOTA = 32 * 1024, CAP_QUOTA = 6 };

	virtual ~Session() { }

	using String = Rpc_in_buffer<Device::DEVICE_NAME_LEN>;

	/**
	 * Request ROM session containing the information about available devices.
	 *
	 * \return  capability to ROM dataspace
	 */
	virtual Rom_session_capability devices_rom() = 0;

	/**
	 * Acquire device known by unique 'name'.
	 */
	virtual Device_capability acquire_device(String const &name) = 0;

	/**
	 * Free server-internal data structures representing the device
	 *
	 * Use this method to relax the resource-allocation of the Platform session.
	 */
	virtual void release_device(Device_capability device) = 0;

	/**
	  * Allocate memory suitable for DMA.
	  */
	virtual Ram_dataspace_capability alloc_dma_buffer(size_t) = 0;

	/**
	 * Free previously allocated DMA memory
	 */
	virtual void free_dma_buffer(Ram_dataspace_capability) = 0;

	/**
	 * Return the bus address of the previously allocated DMA memory
	 */
	virtual addr_t bus_addr_dma_buffer(Ram_dataspace_capability) = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_devices_rom, Rom_session_capability, devices_rom);
	GENODE_RPC_THROW(Rpc_acquire_device, Device_capability, acquire_device,
	                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps),
	                 String const &);
	GENODE_RPC(Rpc_release_device, void, release_device, Device_capability);
	GENODE_RPC_THROW(Rpc_alloc_dma_buffer, Ram_dataspace_capability,
	                 alloc_dma_buffer,
	                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps, Fatal), size_t);
	GENODE_RPC(Rpc_free_dma_buffer, void, free_dma_buffer,
	           Ram_dataspace_capability);
	GENODE_RPC(Rpc_bus_addr_dma_buffer, addr_t, bus_addr_dma_buffer,
	           Ram_dataspace_capability);

	GENODE_RPC_INTERFACE(Rpc_devices_rom, Rpc_acquire_device, Rpc_release_device,
	                     Rpc_alloc_dma_buffer, Rpc_free_dma_buffer,
	                     Rpc_bus_addr_dma_buffer);
};

#endif /* _INCLUDE__SPEC__ARM__PLATFORM_SESSION__PLATFORM_SESSION_H_ */
