/*
 * \brief  C-API Genode USB-client backend
 * \author Sebastian Sumpf
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
#include <genode_c_api/usb.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long genode_usb_client_handle_t;

struct genode_range_allocator;

struct genode_usb_device_descriptor
{
	genode_uint8_t  length;
	genode_uint8_t  type;
	genode_uint16_t usb;
	genode_uint8_t  dclass;
	genode_uint8_t  dsubclass;
	genode_uint8_t  dprotocol;
	genode_uint8_t  max_packet_size;
	genode_uint16_t vendor_id;
	genode_uint16_t product_id;
	genode_uint16_t device_release;
	genode_uint8_t  manufactorer_index;
	genode_uint8_t  product_index;
	genode_uint8_t  serial_number_index;
	genode_uint8_t  num_configs;

	/*
	 * Genode extensions (POD only)
	 */
	unsigned         bus;
	unsigned         num ;
	unsigned         speed;
} __attribute__((packed));


struct genode_usb_config_descriptor
{
	genode_uint8_t  length;
	genode_uint8_t  type;
	genode_uint16_t total_length;
	genode_uint8_t  num_interfaces;
	genode_uint8_t  config_value;
	genode_uint8_t  config_index;
	genode_uint8_t  attributes;
	genode_uint8_t  max_power;
} __attribute__((packed));


genode_usb_client_handle_t
genode_usb_client_create(struct genode_env             *env,
                         struct genode_allocator       *md_alloc,
                         struct genode_range_allocator *alloc,
                         char const                    *label,
                         struct genode_signal_handler  *handler);

void genode_usb_client_destroy(genode_usb_client_handle_t handle,
                               struct genode_allocator *md_alloc);

void genode_usb_client_sigh_ack_avail(genode_usb_client_handle_t handle,
                                      struct genode_signal_handler *handler);

int genode_usb_client_config_descriptor(genode_usb_client_handle_t handle,
                                        struct genode_usb_device_descriptor *device_descr,
                                        struct genode_usb_config_descriptor *config_descr);

bool genode_usb_client_plugged(genode_usb_client_handle_t handle);

void genode_usb_client_claim_interface(genode_usb_client_handle_t handle,
                                       unsigned interface_num);

void genode_usb_client_release_interface(genode_usb_client_handle_t handle,
                                         unsigned interface_num);

struct genode_usb_altsetting
{
	unsigned char interface_number;
	unsigned char alt_setting;
};

struct genode_usb_config
{
	unsigned char value;
};

struct genode_usb_request_packet
{
	unsigned type;
	void   * req;
};

typedef struct genode_usb_request_packet genode_request_packet_t;

struct genode_usb_client_request_packet
{
	genode_request_packet_t  request;
	struct genode_usb_buffer buffer;
	int                      actual_length;
	int                      error;
	void (*complete_callback)(struct genode_usb_client_request_packet *);
	void (*free_callback) (struct genode_usb_client_request_packet *);
	void *completion;
	void *opaque_data;
};

bool genode_usb_client_request(genode_usb_client_handle_t               handle,
                               struct genode_usb_client_request_packet *request);

void genode_usb_client_request_submit(genode_usb_client_handle_t               handle,
                                      struct genode_usb_client_request_packet *request);

void genode_usb_client_request_finish(genode_usb_client_handle_t               handle,
                                      struct genode_usb_client_request_packet *request);

void genode_usb_client_execute_completions(genode_usb_client_handle_t handle);

#ifdef __cplusplus
} /* extern "C" */

#include <base/allocator.h>

struct genode_range_allocator : Genode::Range_allocator { };

static inline auto genode_range_allocator_ptr(Genode::Range_allocator &alloc)
{
	return static_cast<genode_range_allocator *>(&alloc);
}
#endif /* __cplusplus */

#endif /* __GENODE_C_API__USB_CLIENT_H_ */
