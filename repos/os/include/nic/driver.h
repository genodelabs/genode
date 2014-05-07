/*
 * \brief  Interfaces used internally in NIC drivers
 * \author Norman Feske
 * \date   2011-05-21
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NIC__DRIVER_H_
#define _INCLUDE__NIC__DRIVER_H_

#include <os/irq_activation.h>        /* for 'Genode::Irq_handler' type */
#include <nic_session/nic_session.h>  /* for 'Nic::Mac_address' type */

namespace Nic {

	/**
	 * Interface for allocating the backing store for incoming packets
	 */
	struct Rx_buffer_alloc
	{
		/**
		 * Allocate packet buffer
		 */
		virtual void *alloc(Genode::size_t) = 0;

		/**
		 * Submit packet to client
		 */
		virtual void submit() = 0;
	};


	/**
	 * Interface to be implemented by the device-specific driver code
	 */
	struct Driver : Genode::Irq_handler
	{
		/**
		 * Return MAC address of the network interface
		 */
		virtual Mac_address mac_address() = 0;

		/**
		 * Transmit packet
		 *
		 * \param packet start of packet
		 * \param size   packet size
		 *
		 * If the packet size is not a multiple of 4 bytes, this function
		 * accesses the bytes after the packet buffer up to the next 4-byte
		 * length (in the worst case, 3 bytes after the packet end).
		 */
		virtual void tx(char const *packet, Genode::size_t size) = 0;
	};


	/**
	 * Interface for constructing the driver object
	 *
	 * The driver object requires an rx-packet allocator at construction time.
	 * This allocator, however, exists not before the creation of a NIC session
	 * because the client pays for it. Therefore, the driver must be created at
	 * session-construction time. Because drivers may differ with regard to
	 * their constructor arguments, the 'Driver_factory' interface allows for
	 * unifying the session-creation among these drivers.
	 */
	struct Driver_factory
	{
		/**
		 * Construct new driver
		 *
		 * \param rx_buffer_alloc  buffer allocator used for storing incoming
		 *                         packets
		 */
		virtual Driver *create(Rx_buffer_alloc &rx_buffer_alloc) = 0;

		/**
		 * Destroy driver
		 */
		virtual void destroy(Driver *driver) = 0;
	};
}

#endif /* _INCLUDE__NIC__DRIVER_H_ */
