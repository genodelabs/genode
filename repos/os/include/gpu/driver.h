/*
 * \brief  GPU driver interface
 * \author Norman Feske
 * \date   2010-07-28
 *
 * This interface is implemented by the GPU driver and used by the back-end
 * of 'libdrm'. With the forthcoming work on the GPU infrastructure, it will
 * change. It is an intermediate step - please do not use it.
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GPU__DRIVER_H_
#define _GPU__DRIVER_H_

#include <base/stdint.h>

class Gpu_driver
{
	public:

		class Client;

		/**
		 * Create client context
		 */
		virtual Client *create_client() = 0;

		/**
		 * Return PCI device ID of GPU
		 */
		virtual uint16_t device_id() = 0;

		/**
		 * Perform operation of GPU device
		 *
		 * \param request  ioctl opcode relative to 'DRM_COMMAND_BASE'
		 */
		virtual int ioctl(Client *client, int request, void *arg) = 0;

		/**
		 * Map buffer object to local address space
		 *
		 * \param handle  client-local buffer-object handle
		 * \return        base address of mapped buffer object
		 */
		virtual void *map_buffer_object(Client *client, long handle) = 0;

		/**
		 * Remove buffer object from local address space
		 *
		 * \param handle  client-local buffer-object handle
		 */
		virtual void unmap_buffer_object(Client *client, long handle) = 0;
};

/**
 * Obtain GPU driver interface
 */
Gpu_driver *gpu_driver();

#endif /* _GPU__DRIVER_H_ */
