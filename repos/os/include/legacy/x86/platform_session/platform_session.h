/*
 * \brief  Platform session interface
 * \author Norman Feske
 * \date   2008-01-28
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LEGACY__X86__PLATFORM_SESSION__PLATFORM_SESSION_H_
#define _INCLUDE__LEGACY__X86__PLATFORM_SESSION__PLATFORM_SESSION_H_

/* Genode includes */
#include <session/session.h>
#include <base/ram_allocator.h>
#include <base/cache.h>

/* os includes */
#include <legacy/x86/platform_device/platform_device.h>
#include <legacy/x86/platform_device/capability.h>

namespace Platform { struct Session; }


struct Platform::Session : Genode::Session
{
	/**
	 * \noapi
	 */
	static const char *service_name() { return "Platform"; }

	enum { RAM_QUOTA = 16 * 1024, CAP_QUOTA = 2 };

	virtual ~Session() { }

	/**
	 * Find first accessible device
	 */
	virtual Device_capability first_device(unsigned, unsigned) = 0;

	/**
	 * Find next accessible device
	 *
	 * \param prev_device  previous device
	 *
	 * The 'prev_device' argument is used to iterate through all
	 * devices.
	 */
	virtual Device_capability next_device(Device_capability prev_device,
	                                      unsigned, unsigned) = 0; 

	/**
	 * Free server-internal data structures representing the device
	 *
	 * Use this method to relax the heap partition of your PCI session.
	 */
	virtual void release_device(Device_capability device) = 0;

	typedef Rpc_in_buffer<8> Device_name;

	/**
	 * Provide non-PCI device known by unique name
	 */
	virtual Device_capability device(Device_name const &string) = 0;

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

	GENODE_RPC_THROW(Rpc_first_device, Device_capability, first_device,
	                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps),
	                 unsigned, unsigned);
	GENODE_RPC_THROW(Rpc_next_device, Device_capability, next_device,
	                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps),
	                 Device_capability, unsigned, unsigned);
	GENODE_RPC(Rpc_release_device, void, release_device, Device_capability);
	GENODE_RPC_THROW(Rpc_alloc_dma_buffer, Ram_dataspace_capability,
	                 alloc_dma_buffer,
	                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps),
	                 size_t, Cache);
	GENODE_RPC(Rpc_free_dma_buffer, void, free_dma_buffer,
	           Ram_dataspace_capability);
	GENODE_RPC(Rpc_dma_addr, addr_t, dma_addr, Ram_dataspace_capability);
	GENODE_RPC_THROW(Rpc_device, Device_capability, device,
	                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps),
	                 Device_name const &);

	GENODE_RPC_INTERFACE(Rpc_first_device, Rpc_next_device,
	                     Rpc_release_device, Rpc_alloc_dma_buffer,
	                     Rpc_free_dma_buffer, Rpc_dma_addr, Rpc_device);
};

#endif /* _INCLUDE__LEGACY__X86__PLATFORM_SESSION__PLATFORM_SESSION_H_ */
