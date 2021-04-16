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
#include <base/cache.h>
#include <dataspace/capability.h>
#include <rom_session/capability.h>
#include <irq_session/capability.h>
#include <io_mem_session/capability.h>
#include <session/session.h>

namespace Platform {

	using namespace Genode;

	struct Device_interface;
	struct Session;
}


struct Platform::Device_interface : Interface
{
	/**
	 * Byte-offset range of memory-mapped I/O registers within dataspace
	 */
	struct Range { addr_t start; size_t size; };

	GENODE_RPC(Rpc_irq, Irq_session_capability, irq, unsigned);
	GENODE_RPC(Rpc_io_mem, Io_mem_session_capability, io_mem,
	           unsigned, Range &, Cache);

	GENODE_RPC_INTERFACE(Rpc_irq, Rpc_io_mem);
};


struct Platform::Session : Genode::Session
{
	/**
	 * \noapi
	 */
	static const char *service_name() { return "Platform"; }

	enum { RAM_QUOTA = 32 * 1024, CAP_QUOTA = 6 };

	virtual ~Session() { }

	typedef String<64> Device_name;

	/**
	 * Request ROM session containing the information about available devices.
	 *
	 * \return  capability to ROM dataspace
	 */
	virtual Rom_session_capability devices_rom() = 0;

	/**
	 * Acquire device known by unique 'name'
	 */
	virtual Capability<Device_interface> acquire_device(Device_name const &name) = 0;

	/**
	 * Acquire the first resp. single device of this session
	 */
	virtual Capability<Device_interface> acquire_single_device() = 0;

	/**
	 * Free server-internal data structures representing the device
	 *
	 * Use this method to relax the resource-allocation of the Platform session.
	 */
	virtual void release_device(Capability<Device_interface> device) = 0;

	/**
	  * Allocate memory suitable for DMA
	  */
	virtual Ram_dataspace_capability alloc_dma_buffer(size_t, Cache) = 0;

	/**
	 * Free previously allocated DMA memory
	 */
	virtual void free_dma_buffer(Ram_dataspace_capability) = 0;

	/**
	 * Return the bus address of the previously allocated DMA memory
	 */
	virtual addr_t dma_addr(Ram_dataspace_capability) = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_devices_rom, Rom_session_capability, devices_rom);
	GENODE_RPC_THROW(Rpc_acquire_device, Capability<Device_interface>, acquire_device,
	                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps),
	                 Device_name const &);
	GENODE_RPC_THROW(Rpc_acquire_single_device, Capability<Device_interface>,
	                 acquire_single_device, GENODE_TYPE_LIST(Out_of_ram, Out_of_caps));
	GENODE_RPC(Rpc_release_device, void, release_device, Capability<Device_interface>);
	GENODE_RPC_THROW(Rpc_alloc_dma_buffer, Ram_dataspace_capability,
	                 alloc_dma_buffer,
	                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps), size_t, Cache);
	GENODE_RPC(Rpc_free_dma_buffer, void, free_dma_buffer,
	           Ram_dataspace_capability);
	GENODE_RPC(Rpc_dma_addr, addr_t, dma_addr,
	           Ram_dataspace_capability);

	GENODE_RPC_INTERFACE(Rpc_devices_rom, Rpc_acquire_device, Rpc_acquire_single_device,
	                     Rpc_release_device, Rpc_alloc_dma_buffer, Rpc_free_dma_buffer,
	                     Rpc_dma_addr);
};

#endif /* _INCLUDE__SPEC__ARM__PLATFORM_SESSION__PLATFORM_SESSION_H_ */
