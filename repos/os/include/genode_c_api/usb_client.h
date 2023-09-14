/*
 * \brief  C-API Genode USB-client backend
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \date   2023-06-29
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef __GENODE_C_API__USB_CLIENT_H_
#define __GENODE_C_API__USB_CLIENT_H_

#include <base/fixed_stdint.h>
#include <genode_c_api/base.h>
#include <usb_session/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*************************
 ** Lifetime management **
 *************************/

typedef unsigned long genode_usb_client_dev_handle_t;

/**
 * Callback to announce a device
 */
typedef void* (*genode_usb_client_dev_add_t)
	(genode_usb_client_dev_handle_t handle, char const *name,
	 genode_usb_speed_t speed);

/**
 * Callback to delete a device
 */
typedef void (*genode_usb_client_dev_del_t)
	(genode_usb_client_dev_handle_t handle, void *opaque_data);

void genode_usb_client_update(genode_usb_client_dev_add_t add,
                              genode_usb_client_dev_del_t del);

/******************************************
 ** USB device and interface interaction **
 ******************************************/

typedef enum {
	INVALID,
	HALT,
	NO_DEVICE,
	NO_MEMORY,
	TIMEOUT,
	OK
} genode_usb_client_ret_val_t;

/**
 * Callback to produce out content of an USB request
 */
typedef void (*genode_usb_client_produce_out_t)
	(void *opaque_data, genode_buffer_t buffer);

/**
 * Callback to consume in result of an USB request
 */
typedef void (*genode_usb_client_consume_in_t)
	(void *opaque_data, genode_buffer_t buffer);

/**
 * Callback to produce out content of isochronous packet i
 */
typedef genode_uint32_t (*genode_usb_client_produce_out_isoc_t)
	(void *opaque_data, genode_uint32_t i, genode_buffer_t buffer);

/**
 * Callback to consume in result of isochronous packet i
 */
typedef void (*genode_usb_client_consume_in_isoc_t)
	(void *opaque_data, genode_uint32_t i, genode_buffer_t buffer);

/**
 * Callback to complete an USB request
 */
typedef void (*genode_usb_client_complete_t)
	(void *opaque_data, genode_usb_client_ret_val_t result);

genode_usb_client_ret_val_t
genode_usb_client_device_control(genode_usb_client_dev_handle_t handle,
                                 genode_uint8_t                 request,
                                 genode_uint8_t                 request_type,
                                 genode_uint16_t                value,
                                 genode_uint16_t                index,
                                 unsigned long                  size,
                                 void                          *opaque_data);

void
genode_usb_client_device_update(genode_usb_client_produce_out_t      out,
                                genode_usb_client_consume_in_t       in,
                                genode_usb_client_produce_out_isoc_t out_isoc,
                                genode_usb_client_consume_in_isoc_t  in_isoc,
                                genode_usb_client_complete_t         complete);


typedef enum { BULK, IRQ, ISOC, FLUSH } genode_usb_client_iface_type_t;

void
genode_usb_client_claim_interface(genode_usb_client_dev_handle_t handle,
                                  unsigned interface_num);

void
genode_usb_client_release_interface(genode_usb_client_dev_handle_t handle,
                                    unsigned interface_num);

genode_usb_client_ret_val_t
genode_usb_client_iface_transfer(genode_usb_client_dev_handle_t handle,
                                 genode_usb_client_iface_type_t type,
                                 genode_uint8_t                 index,
                                 unsigned long                  size,
                                 void                          *opaque_data);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */


#ifdef __cplusplus

#include <base/allocator.h>
#include <base/env.h>
#include <base/signal.h>

namespace Genode_c_api {

	using namespace Genode;

	/**
	 * Initialize USB client c++ backend
	 */
	void initialize_usb_client(Env                       &env,
	                           Allocator                 &alloc,
	                           Signal_context_capability  io_handler,
	                           Signal_context_capability  rom_handler);
}

#endif /* __cplusplus */

#endif /* __GENODE_C_API__USB_CLIENT_H_ */
